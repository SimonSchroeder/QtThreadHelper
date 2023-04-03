#ifndef THREAD_HELPER_H
#define THREAD_HELPER_H

#include <tuple>

#include <QAbstractEventDispatcher>
#include <QGuiApplication>
#include <QMutex>
#include <QQueue>
#include <QThread>

/**
 * @brief Helper class for invocation of functions in separate thread.
 *
 * WorkerThread has two main roles: First, it provides a single-shot execution
 * of a function in a separate thread. Second, it can run a worker thread with
 * an event queue waiting for functions to be executed in its context.
 *
 * Qt made it easy with QThread::create(...) to temporarily create a thread and
 * run a function. However, there is general boiler-plate code to clean-up after
 * use. Using new and AUTODELETE takes care of everything in a single-shot
 * execution. Use the helper function workerThread(...) directly to launch such
 * a single thread for a function.
 *
 * In order to have a separate worker thread object call the default constructor
 * of WorkerThread. It is your responsibility to clean-up the object when you are
 * done. Execute a function in the context of this thread by calling exec(...).
 * This will take care that the function call is put into the event queue of the
 * thread.
 */
class WorkerThread : public QObject
{
    Q_OBJECT
public:
    enum Type { PERSISTENT,     // Create persistent WorkerThread object which can be re-used.
                DELETETHREAD,   // Delete thread automatically when done. Don't delete WorkerThread object as it might be on the stack.
                AUTODELETE,     // Delete both thread and WorkerThread object when done. The WorkerThread object must be allocated on the heap.
              };
    enum Schedule { PAR,        // Non-serialized, non-blocking
                    ASYNC,      // Default. Serialized, non-blocking
                    SYNC,       // Helper keyword for a synchronous call, e.g. to the GUI thread.
                    JOIN=SYNC,  // Alternative name for a synchronous call.
                    NONBLOCKING_JOIN,   // Does not block event handling of the caller. Joins with the worker thread, i.e. waits for the worker thread to finish.
                  };

private:
    QThread *thread = nullptr;
    Type type;

public:
    // Constructors for persistent objects =====================================
    inline                                      WorkerThread();
    // Constructors with type DELETETHREAD =====================================
    template<class Function>                    WorkerThread(Function &&f);
    template<class Function, typename... Args>  WorkerThread(Function &&f, Args &&... args);
    // Fully parameterized constructors ========================================
    inline                                      WorkerThread(Type type);
    template<class Function>                    WorkerThread(Type type, Function &&f);
    template<class Function, typename... Args>  WorkerThread(Type type, Function &&f, Args &&... args);
    template<class Function>                    WorkerThread(Type type, Schedule schedule, Function &&f);
    template<class Function, typename... Args>  WorkerThread(Type type, Schedule schedule, Function &&f, Args &&...args);
    // Destructor ==============================================================
    inline ~WorkerThread();
    // Deleted functions/constructors ==========================================
    WorkerThread(const WorkerThread &) = delete;
    WorkerThread(WorkerThread &&) = delete;
    const WorkerThread &operator=(const WorkerThread &) = delete;
    const WorkerThread &operator=(WorkerThread &&) = delete;

    // Execute task in persistent WorkerThread object ==========================
    template<class Function>                    void exec(Function &&f);
    template<class Function, typename... Args>  void exec(Function &&f, Args &&... args);
    template<class Function>                    void exec(Schedule schedule, Function &&f);
    template<class Function, typename... Args>  void exec(Schedule schedule, Function &&f, Args &&...args);

signals:
    void done();

//public slots:
public:
    inline void quit();

    /**
     * @brief Helper class for serialization of calls into GUI thread or
     *        within this thread.
     *
     * Serializes function calls into the GUI thread or this thread. By default
     * functions put into the event queue of the GUI thread can overtake each
     * other. This class ensures that only one function at a time is queued into
     * a thread's event loop. Use WorkerThread::getSerializer() to get
     * the single instance for this thread or WorkerThread::getGUISerializer()
     * for a single instance calling GUI functions.
     */
    class Serializer
    {
        /**
         * @brief Internal helper class
         *
         * Actual implementation of the Serializer class. The Serializer object
         * will be deleted when its containing thread finishes. Serializer's
         * destructor makes sure this does not happen when there are still
         * functions in the queue. Those will clean-up in the end if necessary.
         */
        class SerializerImpl
        {
            friend class Serializer;
            QMutex mutex;
            QQueue<std::function<void()>> queue;
            bool running = false;
            bool threadDeleted = false;

        public:
            template<class Function>
            void enqueue(QThread *thread, Function &&f);
            template<class Function, typename... Args>
            void enqueue(QThread *thread, Function &&f, Args &&... args);

            bool isEmpty() const { return queue.isEmpty(); }
        };

        SerializerImpl *impl;
    public:
        Serializer() : impl(new SerializerImpl()) {}
        inline ~Serializer();

        template<class Function>
        void enqueue(QThread *thread, Function &&f)
        { impl->enqueue(thread, std::forward<Function>(f)); }

        template<class Function, typename... Args>
        void enqueue(QThread *thread, Function &&f, Args &&... args)
        { impl->enqueue(thread, std::forward<Function>(f), std::forward<Args>(args)...); }

        /**
         * @brief Check if there are any function calls queued.
         * @return true if queue is empty
         */
        bool isEmpty() const { return impl->isEmpty(); }
    };

    /**
     * @brief Get a single instance of Serializer for each thread.
     * @return current thread's Serializer object
     */
    static inline Serializer &getSerializer()
    {
        thread_local Serializer serializer;
        return serializer;
    }

    /**
     * @brief Get a single instance of Serializer for each thread.
     * @return current thread's GUI Serializer object
     */
    static inline Serializer &getGUISerializer()
    {
        thread_local Serializer serializer;
        return serializer;
    }
};

// HELPER FUNCTIONS ============================================================
template<class Function> inline
WorkerThread *workerThread(Function &&f);
template<class Function, typename... Args> inline
WorkerThread *workerThread(Function &&f, Args &&... args);
template<class Function> inline
WorkerThread *workerThread(WorkerThread::Schedule s, Function &&f);
template<class Function, typename... Args> inline
WorkerThread *workerThread(WorkerThread::Schedule s, Function &&f, Args &&... args);
template<class Function> inline
WorkerThread *workerThreadJoin(Function &&f);
template<class Function, typename... Args> inline
WorkerThread *workerThreadJoin(Function &&f, Args &&... args);

template<class Function> inline
void guiThread(Function &&f);
template<class Function, typename... Args> inline
void guiThread(Function &&f, Args &&... args);
template<class Function> inline
void guiThread(WorkerThread::Schedule schedule, Function &&f);
template<class Function, typename... Args> inline
void guiThread(WorkerThread::Schedule schedule, Function &&f, Args &&... args);

// IMPLEMENTATIONS =============================================================
/**
 * @brief Default constructor creating a persistent worker thread.
 *
 * Create a persistent worker thread. Tasks can be scheduled by calling exec(...).
 * To stop the event queue call quit() on the worker thread.
 */
WorkerThread::WorkerThread()
    : WorkerThread(PERSISTENT)
{
}

/**
 * @brief Call a function with no arguments in a separate worker thread.
 * @param f     function to call
 *
 * Creates a worker thread object and directly calls the function f. When
 * finished done() is emitted and the thread is deleted. WorkerThread can be
 * a stack variable if lifetime is guaranteed to be longer than the thread.
 */
template<class Function>
WorkerThread::WorkerThread(Function &&f)
    : WorkerThread(DELETETHREAD, std::forward(f))
{
}

/**
 * @brief Call a function with arguments in a separate worker thread.
 * @param f     function to call
 * @param args  arguments for f
 *
 * Creates a worker thread object and directly calls the function f with the
 * given arguments args. When finished done() is emitted and the thread is
 * deleted. WorkerThread can be a stack variable if lifetime is guaranteed to
 *  be longer than the thread.
 */
template<class Function, typename... Args>
WorkerThread::WorkerThread(Function &&f, Args &&... args)
    : WorkerThread(DELETETHREAD, std::forward(f), std::forward(args...))
{
}

/**
 * @brief Create an empty worker thread object for later use
 * @param type      variant of worker thread
 *
 * Creates an empty worker thread object of the given type. If the type is
 * WorkerThread::PERSISTENT an event loop is started immediately. Otherwise
 * temporary single-shot threads are created when calling exec(...). In any
 * case functions are executed in the corresponding QThread by calling
 * exec(...). Use this constructor (or the default constructor) if you want
 * to connect to the signal done() which must be done before anything is
 * executed in this thread.
 */
WorkerThread::WorkerThread(Type type)
    : type(type)
{
    switch(type)
    {
    case PERSISTENT:
        thread = new QThread();
        connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
        connect(thread, &QThread::finished, this, &WorkerThread::done);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
        thread->start();
        break;
    case DELETETHREAD:
        break;
    case AUTODELETE:
        break;
    }
}

/**
 * @brief Create worker thread and immediately execute function with no arguments.
 * @param type      variant of worker thread
 * @param f         function to call
 *
 * Create a worker thread object and immediately call the function f. If the
 * type is WorkerThread::PERSISTENT more functions can be executed by calling
 * exec(...).
 */
template<class Function>
WorkerThread::WorkerThread(Type type, Function &&f)
    : WorkerThread(type, ASYNC, std::forward<Function>(f))
{
}

/**
 * @brief Create worker thread and immediately execute function with arguments.
 * @param type      variant of worker thread
 * @param f         function to call
 * @param args      arguments for f
 *
 * Create a worker thread object and immediately call the function f with the
 * arguments args. If the type is WorkerThread::PERSISTENT more functions can
 * be executed by calling exec(...).
 */
template<class Function, typename... Args>
WorkerThread::WorkerThread(Type type, Function &&f, Args &&... args)
    : WorkerThread(type, ASYNC, std::forward<Function>(f), std::forward<Args>(args)...)
{
}

/**
 * @brief Create worker thread and immediately execute function with no arguments.
 * @param type      variant of worker thread
 * @param s         schedule of execution – e.g. use NONBLOCKING_JOIN to wait without blocking
 * @param f         function to call
 *
 * Create a worker thread object and immediately call the function f. If the
 * type is WorkerThread::PERSISTENT more functions can be executed by calling
 * exec(...). Use schedule WorkerThread::JOIN for a blocking join or
 * WorkerThread::NONBLOCKING_JOIN to wait without blocking.
 */
template<class Function>
WorkerThread::WorkerThread(Type type, Schedule schedule, Function &&f)
    : type(type)
{
    QMutex syncMutex;       // for SYNC/JOIN
    if(schedule == SYNC)
        syncMutex.lock();

    QEventLoop eventLoop;   // for NONBLOCKING_JOIN

    // Ignore PAR/ASYNC as for a single function call it is the same.

    switch(type)
    {
    case PERSISTENT:
        thread = new QThread();
        connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
        connect(thread, &QThread::finished, this, &WorkerThread::done);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
        thread->start();
        this->exec(schedule, std::forward<Function>(f));
        break;
    case DELETETHREAD:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)]() mutable
        {
            if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
            if(schedule == SYNC)
                syncMutex.unlock();
        });
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)]() mutable
        {
            if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
            if(schedule == SYNC)
                syncMutex.unlock();
        });
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        break;
    }

    if(schedule == SYNC)
    {
        syncMutex.lock();       // wait for execution of function within thread to finish
        syncMutex.unlock();
    }
}

/**
 * @brief Create worker thread and immediately execute function with arguments.
 * @param type      variant of worker thread
 * @param s         schedule of execution – e.g. use NONBLOCKING_JOIN to wait without blocking
 * @param f         function to call
 * @param args      arguments for f
 *
 * Create a worker thread object and immediately call the function f with the
 * arguments args. If the type is WorkerThread::PERSISTENT more functions can
 * be executed by calling exec(...). Use schedule WorkerThread::JOIN for a
 * blocking join or WorkerThread::NONBLOCKING_JOIN to wait without blocking.
 */
template<class Function, typename... Args>
WorkerThread::WorkerThread(Type type, Schedule schedule, Function &&f, Args &&... args)
{
    QMutex syncMutex;           // for SYNC/JOIN
    if(schedule == SYNC)
        syncMutex.lock();

    QEventLoop eventLoop;       // for NONBLOCKING_JOIN

    switch(type)
    {
    case PERSISTENT:
        thread = new QThread();
        connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
        connect(thread, &QThread::finished, this, &WorkerThread::done);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
        thread->start();
        this->exec(schedule, std::forward<Function>(f), std::forward<Args>(args)...);
        break;
    case DELETETHREAD:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)](auto &&... args) mutable
        {if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
            if(schedule == SYNC)
                syncMutex.unlock();
        }, std::forward<Args>(args)...);
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)](auto &&... args) mutable
        {
            if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
            if(schedule == SYNC)
                syncMutex.unlock();
        }, std::forward<Args>(args)...);
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        break;
    }

    if(schedule == SYNC)
    {
        syncMutex.lock();       // wait for execution of function within thread to finish
        syncMutex.unlock();
    }
}

WorkerThread::~WorkerThread()
{
}

/**
 * @brief Execute a function with no arguments in the threads context.
 * @param f         function to call
 *
 * Executes the function f in the context of this worker thread. For the
 * WorkerThread::PERSISTENT variant the function call will be queued in the
 * event queue. This serializes and synchronizes function calls within this
 * thread.
 */
template<class Function>
void WorkerThread::exec(Function &&f)
{
    this->exec(ASYNC, std::forward<Function>(f));
}

/**
 * @brief Execute a function with no arguments in the threads context.
 * @param f         function to call
 * @param args      arguments for f
 *
 * Executes the function f in the context of this worker thread. For the
 * WorkerThread::PERSISTENT variant the function call will be queued in the
 * event queue. This serializes and synchronizes function calls within this
 * thread.
 */
template<class Function, typename... Args>
void WorkerThread::exec(Function &&f, Args &&... args)
{
    this->exec(ASYNC, std::forward<Function>(f), std::forward<Args>(args)...);
}

/**
 * @brief Execute a function with no arguments in the threads context.
 * @param schedule  schedule to use
 * @param f         function to call
 *
 * Executes the function f in the context of this worker thread. For the
 * WorkerThread::PERSISTENT variant the function call will be queued in the
 * event queue. This serializes and synchronizes function calls within this
 * thread. Different schedules can be used, e.g. to wait for the execution
 * to finish.
 */
template<class Function>
void WorkerThread::exec(Schedule schedule, Function &&f)
{
    QMutex syncMutex;       // for SYNC/JOIN
    if(schedule == SYNC)
        syncMutex.lock();

    QEventLoop eventLoop;   // for NONBLOCKING_JOIN

    switch(type)
    {
    case PERSISTENT:
        // Wait for running event queue if we just started the thread.
        while(!QAbstractEventDispatcher::instance(thread))
            QThread::currentThread()->msleep(1);
        switch(schedule)
        {
        case PAR:
            QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread), std::forward<Function>(f), Qt::QueuedConnection);
            break;
        case ASYNC:
            getSerializer().enqueue(this->thread, std::forward<Function>(f));
            break;
        case SYNC:
            QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                      [f=std::forward<Function>(f),&syncMutex]() mutable
                                      {
                                        std::invoke(std::forward<Function>(f));
                                        syncMutex.unlock();
                                      },
                                      Qt::QueuedConnection);
            break;
        case NONBLOCKING_JOIN:
            QMetaObject::invokeMethod(&eventLoop,
                                      [this,&eventLoop,f=std::forward<Function>(f)]() mutable
                                      {
                                        QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                                                  [&eventLoop,f=std::forward<Function>(f)]() mutable
                                                                  {
                                                                    std::invoke(std::forward<Function>(f));
                                                                    eventLoop.exit();
                                                                  },
                                                                  Qt::QueuedConnection);
                                      },
                                      Qt::QueuedConnection);
            eventLoop.exec();
            break;
        }
        break;
    case DELETETHREAD:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)]() mutable
        {
            if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
            if(schedule == SYNC)
                syncMutex.unlock();
        });
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)]() mutable
        {
            if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
            if(schedule == SYNC)
                syncMutex.unlock();
        });
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        thread->start();
        break;
    }

    if(schedule == SYNC)
    {
        syncMutex.lock();
        syncMutex.unlock();
    }
}

/**
 * @brief Execute a function with no arguments in the threads context.
 * @param schedule  schedule to use
 * @param f         function to call
 * @param args      arguments for f
 *
 * Executes the function f in the context of this worker thread. For the
 * WorkerThread::PERSISTENT variant the function call will be queued in the
 * event queue. This serializes and synchronizes function calls within this
 * thread. Different schedules can be used, e.g. to wait for the execution
 * to finish.
 */
template<class Function, typename... Args>
void WorkerThread::exec(Schedule schedule, Function &&f, Args &&... args)
{
    QMutex syncMutex;       // for SYNC/JOIN
    if(schedule == SYNC)
        syncMutex.lock();

    QEventLoop eventLoop;   // for NONBLOCKING_JOIN

    switch(type)
    {
    case PERSISTENT:
        // Wait for running event queue if we just started the thread.
        while(!QAbstractEventDispatcher::instance(thread))
            QThread::currentThread()->msleep(1);
        switch(schedule)
        {
        case PAR:
            QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                      [f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...)]() mutable
                                      {
                                        std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                                      },
                                      Qt::QueuedConnection);
            break;
        case ASYNC:
            getSerializer().enqueue(this->thread, std::forward<Function>(f), std::forward<Args>(args)...);
            break;
        case SYNC:
            QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                      [f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...),&syncMutex]() mutable
                                      {
                                        std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                                        syncMutex.unlock();
                                      },
                                      Qt::QueuedConnection);
            break;
        case NONBLOCKING_JOIN:
            QMetaObject::invokeMethod(&eventLoop,
                                      [this,&eventLoop,f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...)]() mutable
                                      {
                                        QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                                                  [&eventLoop,f=std::forward<Function>(f),args=std::forward<decltype(args)>(args)]() mutable
                                                                  {
                                                                    std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                                                                    eventLoop.exit();
                                                                  },
                                                                  Qt::QueuedConnection);
                                      },
                                      Qt::QueuedConnection);
            eventLoop.exec();
            break;
        }
        break;
    case DELETETHREAD:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)](auto &&... args) mutable
        {
            if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
            if(schedule == SYNC)
                syncMutex.unlock();
        }, std::forward<Args>(args)...);
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,schedule,&syncMutex,&eventLoop,f=std::forward<Function>(f)](auto &&... args) mutable
        {
            if(schedule == NONBLOCKING_JOIN)
                connect(thread, &QThread::finished, [&eventLoop]() { eventLoop.exit(); });
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
            if(schedule == SYNC)
                syncMutex.unlock();
        }, std::forward<Args>(args)...);
        if(schedule == NONBLOCKING_JOIN)
        {
            QMetaObject::invokeMethod(&eventLoop, [this]() { this->thread->start(); }, Qt::QueuedConnection);
            eventLoop.exec();
        }
        else
            thread->start();
        break;
    }

    if(schedule == SYNC)
    {
        syncMutex.lock();
        syncMutex.unlock();
    }
}

/**
 * @brief Quit the execution of the worker thread.
 *
 * Quits the event loop for the WorkerThread::PERSISTENT variant. Otherwise
 * this function does nothing. Note that it will not stop running or scheduled
 * function calls.
 */
void WorkerThread::quit()
{
    thread->quit();
}

/**
 * @brief Run a function asynchronously.
 * @param f         function to call
 *
 * Similar to QThread::create(f)->start(). However, this call manages lifetime
 * of the underlying thread automatically. Unlike QtConcurrent::run(...) this
 * launches a separate thread and executes the function immediately. There is
 * no future and thus no possible return type.
 */
template<class Function> inline
WorkerThread *workerThread(Function &&f)
{
    return new WorkerThread(WorkerThread::AUTODELETE, std::forward<Function>(f));
}

/**
 * @brief Run a function with arguments asynchronously.
 * @param f         function to call
 * @param args      arguments for f
 *
 * Similar to QThread::create(f, args...)->start(). However, this call manages
 * lifetime of the underlying thread automatically. Unlike
 * QtConcurrent::run(...) this launches a separate thread and executes the
 * function immediately. There is no future and thus no possible return type.
 */
template<class Function, typename... Args> inline
WorkerThread *workerThread(Function &&f, Args &&... args)
{
    return new WorkerThread(WorkerThread::AUTODELETE, std::forward<Function>(f), std::forward<Args>(args)...);
}

/**
 * @brief Run a function in a separate thread with a given schedule.
 * @param schedule  schedule to use
 * @param f         function to call
 *
 * Starts a new thread in the same way as workerThread(f). However, a specific
 * schedule is used, e.g. JOIN or NONBLOCKING_JOIN.
 */
template<class Function> inline
WorkerThread *workerThread(WorkerThread::Schedule schedule, Function &&f)
{
    return new WorkerThread(WorkerThread::AUTODELETE, schedule, std::forward<Function>(f));
}

/**
 * @brief Run a function with arguments in a separate thread with a given schedule.
 * @param schedule  schedule to use
 * @param f         function to call
 * @param args      arguments for f
 *
 * Starts a new thread in the same way as workerThread(f, args). However, a
 * specific schedule is use, e.g. JOIN or NONBLOCKING_JOIN.
 */
template<class Function, typename... Args> inline
WorkerThread *workerThread(WorkerThread::Schedule schedule, Function &&f, Args &&...args)
{
    return new WorkerThread(WorkerThread::AUTODELETE, schedule, std::forward<Function>(f), std::forward<Args>(args)...);
}

/**
 * @brief Run a function in a separate thread and wait, but don't block event processing.
 * @param f         function to call
 *
 * Starts a new thread in the same way as workerThread(f). However, a local event loop
 * is used to wait for the thread to finish without blocking event processing.
 */
template<class Function> inline
WorkerThread *workerThreadJoin(Function &&f)
{
    return new WorkerThread(WorkerThread::AUTODELETE, WorkerThread::NONBLOCKING_JOIN, std::forward<Function>(f));
}

/**
 * @brief Run a function with arguments in a separate thread and wait, but don't block event processing.
 * @param f         function to call
 * @param args      arguments for f
 *
 * Starts a new thread in the same way as workerThread(f, args). However, a local event loop
 * is used to wait for the thread to finish without blocking event processing.
 */
template<class Function, typename... Args> inline
WorkerThread *workerThreadJoin(Function &&f, Args &&... args)
{
    return new WorkerThread(WorkerThread::AUTODELETE, WorkerThread::NONBLOCKING_JOIN, std::forward<Function>(f), std::forward<Args>(args)...);
}

/**
 * @brief Call a function in the original GUI thread.
 * @param f         function to call
 *
 * Helpful function when running in a separate thread. This will execute the
 * given function inside the main GUI thread. Use this call when updating
 * information in widgets.
 */
template<class Function> inline
void guiThread(Function &&f)
{
    guiThread(WorkerThread::ASYNC, std::forward<Function>(f));
}

/**
 * @brief Call a function in the original GUI thread.
 * @param f         function to call
 * @param args      arguments for f
 *
 * Helpful function when running in a separate thread. This will execute the
 * given function together with its arguments inside the main GUI thread. Use
 * this call when updating information in widgets.
 */
template<class Function, typename... Args> inline
void guiThread(Function &&f, Args &&... args)
{
    guiThread(WorkerThread::ASYNC, std::forward<Function>(f), std::forward<Args>(args)...);
}

/**
 * @brief Overload for a blocking call of a function in a GUI thread.
 * @param f         function to call
 *
 * Call this function with WorkerThread::SYNC to execute the function f
 * and block until the call is done. Call with WorkerThread::PAR (for parallel)
 * if the order of execution does not matter.
 */
template<class Function> inline
void guiThread(WorkerThread::Schedule schedule, Function &&f)
{
    switch(schedule)
    {
    case WorkerThread::PAR:
        QMetaObject::invokeMethod(qGuiApp, f, Qt::QueuedConnection);
        break;
    case WorkerThread::ASYNC:
        WorkerThread::getGUISerializer().enqueue(nullptr, std::forward<Function>(f));
        break;
    case WorkerThread::SYNC:
        {
            QMutex syncMutex;
            syncMutex.lock();   // lock for GUI thread
            QMetaObject::invokeMethod(qGuiApp,
                                      [f=std::forward<Function>(f),&syncMutex]() mutable
                                      {
                                        std::invoke(std::forward<Function>(f));
                                        syncMutex.unlock();
                                      },
                                      Qt::QueuedConnection);
            syncMutex.lock();   // wait for GUI call to finish
            syncMutex.unlock();
        }
        break;
    case WorkerThread::NONBLOCKING_JOIN:
        {
            QEventLoop eventLoop;
            QMetaObject::invokeMethod(&eventLoop,
                                      [&eventLoop,f=std::forward<Function>(f)]() mutable
                                      {
                                        QMetaObject::invokeMethod(qApp,
                                                                  [&eventLoop,f=std::forward<Function>(f)]() mutable
                                                                  {
                                                                    std::invoke(std::forward<Function>(f));
                                                                    eventLoop.exit();
                                                                  },
                                                                  Qt::QueuedConnection);
                                      },
                                      Qt::QueuedConnection);
            eventLoop.exec();
        }
        break;
    }
}

/**
 * @brief Overload for a blocking call of a function in a GUI thread.
 * @param f         function to call
 * @param args      arguments for f
 *
 * Call this function with WorkerThread::SYNC to execute the function f with
 * arguments args and block until the call is done. Call with WorkerThread::PAR
 * (for parallel) if the order of execution does not matter.
 */
template<class Function, typename... Args> inline
void guiThread(WorkerThread::Schedule schedule, Function &&f, Args &&... args)
{
    switch(schedule)
    {
    case WorkerThread::PAR:
        QMetaObject::invokeMethod(qGuiApp,
                                  [f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...)]() mutable
                                  {
                                    std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                                  },
                                  Qt::QueuedConnection);
        break;
    case WorkerThread::ASYNC:
        WorkerThread::getGUISerializer().enqueue(nullptr, std::forward<Function>(f), std::forward<Args>(args)...);
        break;
    case WorkerThread::SYNC:
        {
            QMutex syncMutex;
            syncMutex.lock();   // lock for GUI thread
            QMetaObject::invokeMethod(qGuiApp,
                                      [f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...),&syncMutex]() mutable
                                      {
                                        std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                                        syncMutex.unlock();
                                      },
                                      Qt::QueuedConnection);
            syncMutex.lock();   // wait for GUI call to finish
            syncMutex.unlock();
        }
        break;
    case WorkerThread::NONBLOCKING_JOIN:
        {
            QEventLoop eventLoop;
            QMetaObject::invokeMethod(&eventLoop,
                                      [&eventLoop,f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...)]() mutable
                                      {
                                        QMetaObject::invokeMethod(qApp,
                                                                  [&eventLoop,f=std::forward<Function>(f),args=std::forward<decltype(args)>(args)]() mutable
                                                                  {
                                                                    std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                                                                    eventLoop.exit();
                                                                  },
                                                                  Qt::QueuedConnection);
                                      },
                                      Qt::QueuedConnection);
            eventLoop.exec();
        }
        break;
    }
}

/**
 * @brief Queue a new function call to be executed serialized for this thread.
 * @param thread    QThread object if serialized on WorkerThread's event loop
 * @param f         function to call
 *
 * This will queue new function calls to be executed in the GUI's event loop.
 * If no other function from this thread is already executing, the current
 * function will be pushed to the GUI's event loop directly. When done the
 * function looks for further functions in the queue and sends the next one
 * to the GUI thread. If no functions are remaining in the queue and the thread
 * has already finished, the underlying SerializerImpl object is deleted.
 */
template<class Function>
void WorkerThread::Serializer::SerializerImpl::enqueue(QThread *thread, Function &&f)
{
    auto lambda = [f=std::forward<Function>(f),this,thread]() mutable
    {
        std::invoke(std::forward<Function>(f));

        this->mutex.lock();
        if(this->isEmpty())
        {
            running = false;
            if(threadDeleted)
            {
                this->mutex.unlock();
                delete this;
                return;
            }
        }
        else
        {
            if(thread == nullptr)
                QMetaObject::invokeMethod(qGuiApp, this->queue.dequeue(), Qt::QueuedConnection);
            else
                QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                          this->queue.dequeue(),
                                          Qt::QueuedConnection);
        }
        this->mutex.unlock();
    };

    this->mutex.lock();
    if(this->isEmpty() && !running)
    {
        running = true;
        if(thread == nullptr)
            QMetaObject::invokeMethod(qGuiApp, lambda, Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                      lambda,
                                      Qt::QueuedConnection);
    }
    else
    {
        this->queue.enqueue(lambda);
    }
    this->mutex.unlock();
}

/**
 * @brief Queue a new function call to be executed serialized for this thread.
 * @param thread    QThread object if serialized on WorkerThread's event loop
 * @param f         function to call
 * @param args      arguments to f
 *
 * This will queue new function calls to be executed in the GUI's event loop.
 * If no other function from this thread is already executing, the current
 * function will be pushed to the GUI's event loop directly. When done the
 * function looks for further functions in the queue and sends the next one
 * to the GUI thread. If no functions are remaining in the queue and the thread
 * has already finished, the underlying SerializerImpl object is deleted.
 */
template<class Function, typename... Args>
void WorkerThread::Serializer::SerializerImpl::enqueue(QThread *thread, Function &&f, Args &&... args)
{
    auto lambda = [f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...),this,thread]() mutable
    {
        std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));

        this->mutex.lock();
        if(this->isEmpty())
        {
            running = false;
            if(threadDeleted)
            {
                this->mutex.unlock();
                delete this;
                return;
            }
        }
        else
        {
            if(thread == nullptr)
                QMetaObject::invokeMethod(qGuiApp, this->queue.dequeue(), Qt::QueuedConnection);
            else
                QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                          this->queue.dequeue(),
                                          Qt::QueuedConnection);
        }
        this->mutex.unlock();
    };

    this->mutex.lock();
    if(this->isEmpty() && !running)
    {
        running = true;
        if(thread == nullptr)
            QMetaObject::invokeMethod(qGuiApp, lambda, Qt::QueuedConnection);
        else
            QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                      lambda,
                                      Qt::QueuedConnection);
    }
    else
    {
        this->queue.enqueue(lambda);
    }
    this->mutex.unlock();
}

/**
 * @brief Destructor for the Serializer's per-thread object.
 *
 * Given that only one Serializer object is used per thread (by calling
 * getGUISerializer() instead of the constructor) the destructor is called when
 * the thread finishes. The destructor only cleans up if there are no more
 * functions in the queue and none are still running. Otherwise, the lambda
 * functions wrapping the actual function call will clean up everything when
 * done.
 */
WorkerThread::Serializer::~Serializer()
{
    impl->mutex.lock();
    if(impl->queue.isEmpty() && !impl->running)
    {
        impl->threadDeleted = true;
        impl->mutex.unlock();
        delete impl;
    }
    else
    {
        impl->threadDeleted = true;
        impl->mutex.unlock();
    }
}

#endif // THREAD_HELPER_H
