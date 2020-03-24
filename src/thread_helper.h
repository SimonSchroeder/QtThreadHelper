#ifndef THREAD_HELPER_H
#define THREAD_HELPER_H

#include <tuple>

#include <QAbstractEventDispatcher>
#include <QGuiApplication>
#include <QMutex>
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
 * a single-shot thread for a function.
 *
 * In order to have a separate worker thread object call the default constructor
 * of WorkerThread. It is your responsibility to clean-up the object when your
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
                AUTODELETE      // Delete both thread and WorkerThread object when done. The WorkerThread object must be allocated on the heap.
              };
    enum Schedule { ASYNC,      // Default.
                    SYNC        // Helper keyword for a synchronous call to the GUI thread.
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

signals:
    void done();

//public slots:
public:
    inline void quit();
};

// HELPER FUNCTIONS ============================================================
template<class Function> inline
WorkerThread *workerThread(Function &&f);
template<class Function, typename... Args> inline
WorkerThread *workerThread(Function &&f, Args &&... args);

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
        this->exec(std::forward<Function>(f));
        break;
    case DELETETHREAD:
        thread = QThread::create([this,f=std::forward<Function>(f)]() mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
        });
        thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,f=std::forward<Function>(f)]() mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
        });
        thread->start();
        break;
    }
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
{
    switch(type)
    {
    case PERSISTENT:
        thread = new QThread();
        connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
        connect(thread, &QThread::finished, this, &WorkerThread::done);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
        thread->start();
        this->exec(std::forward<Function>(f), std::forward<Args>(args)...);
        break;
    case DELETETHREAD:
        thread = QThread::create([this,f=std::forward<Function>(f)](auto &&... args) mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
        }, std::forward<Args>(args)...);
        thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,f=std::forward<Function>(f)](auto &&... args) mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
        }, std::forward<Args>(args)...);
        thread->start();
        break;
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
    switch(type)
    {
    case PERSISTENT:
        // Wait for running event queue if we just started the thread.
        while(!QAbstractEventDispatcher::instance(thread))
            QThread::currentThread()->msleep(1);
        QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread), std::forward<Function>(f), Qt::QueuedConnection);
        break;
    case DELETETHREAD:
        thread = QThread::create([this,f=std::forward<Function>(f)]() mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
        });
        thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,f=std::forward<Function>(f)]() mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f));
        });
        thread->start();
        break;
    }
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
    switch(type)
    {
    case PERSISTENT:
        // Wait for running event queue if we just started the thread.
        while(!QAbstractEventDispatcher::instance(thread))
            QThread::currentThread()->msleep(1);
        QMetaObject::invokeMethod(QAbstractEventDispatcher::instance(thread),
                                  [f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...)]() mutable
                                  {
                                    std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                                  },
                                  Qt::QueuedConnection);
        break;
    case DELETETHREAD:
        thread = QThread::create([this,f=std::forward<Function>(f)](auto &&... args) mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
        }, std::forward<Args>(args)...);
        thread->start();
        break;
    case AUTODELETE:
        thread = QThread::create([this,f=std::forward<Function>(f)](auto &&... args) mutable
        {
            connect(thread, &QThread::finished, [this]() { this->thread = nullptr; });
            connect(thread, &QThread::finished, this, &WorkerThread::done);
            connect(this,  &WorkerThread::done, [this]() { delete this; });
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            std::invoke(std::forward<Function>(f), std::forward<Args>(args)...);
        }, std::forward<Args>(args)...);
        thread->start();
        break;
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
    QMetaObject::invokeMethod(qGuiApp, f, Qt::QueuedConnection);
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
    QMetaObject::invokeMethod(qGuiApp,
                              [f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...)]() mutable
                              {
                                std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                              },
                              Qt::QueuedConnection);
}

/**
 * @brief Overload for a blocking call of a function in a GUI thread.
 * @param f         function to call
 *
 * Call this function with WorkerThread::SYNC to execute the function f
 * and block until the call is done.
 */
template<class Function> inline
void guiThread(WorkerThread::Schedule schedule, Function &&f)
{
    switch(schedule)
    {
    case WorkerThread::ASYNC:
        guiThread(std::forward<Function>(f));
        break;
    case WorkerThread::SYNC:
        {
            QMutex syncMutex;
            syncMutex.lock();   // lock for GUI thread
            guiThread([f=std::forward<Function>(f),&syncMutex]() mutable
                      {
                        std::invoke(std::forward<Function>(f));
                        syncMutex.unlock();
                      });
            syncMutex.lock();   // wait for GUI call to finish
            syncMutex.unlock();
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
 * arguments args and block until the call is done.
 */
template<class Function, typename... Args> inline
void guiThread(WorkerThread::Schedule schedule, Function &&f, Args &&... args)
{
    switch(schedule)
    {
    case WorkerThread::ASYNC:
        guiThread(std::forward<Function>(f), std::forward<Args>(args)...);
        break;
    case WorkerThread::SYNC:
        {
            QMutex syncMutex;
            syncMutex.lock();   // lock for GUI thread
            guiThread([f=std::forward<Function>(f),args=std::make_tuple(std::forward<Args>(args)...),&syncMutex]() mutable
                      {
                        std::apply(std::forward<Function>(f), std::forward<decltype(args)>(args));
                        syncMutex.unlock();
                      });
            syncMutex.lock();   // wait for GUI call to finish
            syncMutex.unlock();
        }
        break;
    }
}

#endif // THREAD_HELPER_H
