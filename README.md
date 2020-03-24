# QtThreadHelper
Some QThread helper classes and functions for specific use cases (single header-only library, depends on Qt)

## TL;DR
First I give a short overview how to use this header-only library. It does __not__ make sense to use it as a general replacement for other ways of using threads. For infos when it makes sense read on further below.
### What you need
Copy `thread_helper.h` from `src` and add it to your project. Make sure that your compiler has C++17 support turned on (tested with VS2019). E.g. if you are using a QMake project file add the line
```
CONFIG += c++17
```
Then include the header where you want to use these functions:
```
#include "thread_helper.h"
```

### How to use
1. Create a single-shot temporary thread by calling `workerThread([](){ /* do stuff */ });`.
1. Queue function calls into the GUI thread (when manipulating GUI elements) from another thread by calling `guiThread([]() { /* call function on widget */ });`.
1. Create a permanent worker thread object `WorkerThread *wt = new WorkerThread()` with its own event queue and continuously queue function calls: `wt->exec([]() { /* do stuff */ })`. Stop the event queue with `wt->quit()`.
1. Easily use function arguments in all of the above, e.g. `workerThread([](int i) { /* do stuff */ }, 1);`.
1. Use member functions instead of lambdas in any of the above, e.g. `guiThread(std::mem_fn(&MainWindow::setWindowTitle), this, "Title");`.

## About
While rewriting a lot of code some pattern of using `QThread` slowly emerged. It is not the best way in all cases, but it allowed us to easily put blocks of code into a lambda function and move the lambda to different thread. However, in all instances we rewrote the same boilerplate code for final thread clean-up over and over again. This is the reason for this small helper library.

In contrast to `QtConcurrent::run` you will not take part in a thread pool, but instead always launch your own thread. Furthermore, you do not have a future object and thus cannot return a value. Instead you would queue another function call in one of the event queues you might have. (There is at least the main event queue running.)

You could, however, use `QThread` and `QThread::create` directly. This small lib does exactly that. But, there is a lot you would need to remember (maybe you have even read [this article](https://mayaposch.wordpress.com/2011/11/01/how-to-really-truly-use-qthreads-the-full-explanation/)). I always have to go back to the places I used this before to get it exactly right. This is the reason why I chose to write this small library around the usual function calls.

__Warning__ If you use this approach in several places in your code you should make sure that you do not oversubscribe you CPU cores with too many threads. The `WorkerThread` class with its default constructor replaces `QThread` directly for certain use cases. So, the same rules apply in this case. For the lifetime of this object you have a thread hanging around somewhere. The function `workerThread(...)` (small letter 'w') is similar to `QThread::create(...)->start()`: It creates a temporary thread to execute just this one function. If you do this kind of offloading everywhere for longer computational tasks you might get too many threads. I suggest only using this from functions triggered by GUI interaction. There are two use cases: 1) The task takes about 10 seconds. The user will no interact fast enough to trigger too many threads. 2) There is a modal dialog while running the threads. The user cannot start further threads.

## Use case 1
Suppose you have an already existing function
```c++
void MyClass::foo()
{
    /* do some stuff */
    QThread::sleep(10); // simulate work by sleeping 10 seconds
}
```
If called by clicking a button or something, this will freeze the GUI for 10 seconds. Some of the time it is sufficient to just move the whole function into a lambda and call that in a different thread:
```c++
void MyClass::foo()
{
    workerThread([this]()
    {
        /* do some stuff */
        QThread::sleep(10); // simulate work
    });
}
```
`workerThread(...)` calls `QThread::create(...)->start()` and also makes sure to clean-up the thread after itself. In the simplest example the whole function body is put inside the lambda.

## Use case 2
Often we have to update the GUI after doing something. One prominent example is using a `QProgressDialog` which needs to update while the thread is running. Let's start off by a progress dialog in the GUI thread:
```c++
void MyClass::compute()
{
    QProgressDialog progress(this);
    progress.setWindowModality(Q::WindowModal);
    
    for(int i = 0; i < 10; ++i)
    {
        /* do some stuff */
        QThread::sleep(1);              //simulate work
        progress.setValue(10 * (i+1));  // convert to percent
    }
}
```
If you have done it like this, you probably also already experienced that it is not possible to cancel because the GUI thread is blocking. Maybe you know the trick to call `QApplication::processEvents();` after `progress.setValue(...)`. This can significantly slow down you computation (even `processEvents` with a timeout). Next trick is to do this only like every 100 iteration (if you have a few thousand). But, even this might be slow and the cancel button of your progress dialog might not react on every click. Been there, done all of that.

Best trick is to do the computation in a separate thread and let the GUI thread running. Now, we need to call back to the GUI thread for the progress dialog. This would look like the following:
```c++
void MyClass::compute()
{
    // put everything into a worker thread
    workerThread([this]()
    {
        QProgressDialog *progress;
        guiThread(WorkerThread::SYNC, [this,&progress]()
        {
            progress = new QProgressDialog(this);
            progress->setWindowModality(Qt::WindowModel);
        });
        
        for(int i = 0; i < 10; ++i)
        {
            /* do some stuff */
            QThread::sleep(1); //simulate work
            guiThread([progress,i]() { progress.setValue(10 * (i+1)); }); // needs to be executed in GUI thread
        }
        
        guiThread([progress]() { delete progress; }); // don't forget to clean up
    });
}
```
Using `guiThread(...)` executes the function in the GUI thread by using `QMetaObject::invokeMethod(qApp, ..., Qt::QueuedConnection)`. The Qt way is hard to remember and tedious to write. Hence, this lib provides a shortcut `guiThread` which is also more explicit about what we are trying to achieve here.

`workerThread(...)` internally uses the class `WorkerThread` (note the difference between small 'w' and capital 'W'). The lib's enums live inside this class. The construction of the `QProgressDialog` needs to finish before we do anything else with it. This is done by providing the parameter `WorkerThread::SYNC` to `guiThread`. Internally, it uses a `QMutex` to synchronize the GUI thread and this thread. So, it is basically a blocking call into the GUI thread (make sure that the GUI thread is always progressing). Now, the progress dialog can even react to the cancel button directly without any problems (we still need to handle it inside our computation, though). Because of modality the user has to wait for the computation to finish, but the GUI stays responsive (for the progress dialog) and the computation is a lot faster than calling `QApplication::processEvents()`.

This use case is highly encouraged.
