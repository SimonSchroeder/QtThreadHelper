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

You could, however, use `QThread` and `QThread::create` directly. This small lib does exactly that. But, there is a lot you would need to remember. I always have to go back to the places I used this before to get it exactly right. This is the reason why I chose to write this small library around the usual function calls.

__Warning__ If you use this approach in several places in your code you should make sure that you do not oversubscribe you CPU cores with too many threads. The `WorkerThread` class with its default constructor replaces `QThread` directly for certain use cases. So, the same rules apply in this case. For the lifetime of this object you have a thread hanging around somewhere. The function `workerThread(...)` (small letter 'w') is similar to `QThread::create(...)->start()`: It creates a temporary thread to execute just this one function. If you do this kind of offloading everywhere for longer computational tasks you might get too many threads. I suggest only using this from functions triggered by GUI interaction. There are two use cases: 1) The task takes about 10 seconds. The user will no interact fast enough to trigger too many threads. 2) There is a modal dialog while running the threads. The user cannot start further threads.

## Use case 1
Suppose you have an already existing function
```c++
void MyClass::foo()
{
    /* do some stuff */
    QThread::currentThread()->sleep(10); // simulate work by sleeping 10 seconds
}
```
If called by clicking a button or something, this will freeze the GUI for 10 seconds. Some of the time it is sufficient to just move the whole function into a lambda and call that in a different thread:
```c++
void MyClass::foo()
{
   workerThread([this]()
   {
       /* do some stuff */
       QThread::currentThread()->sleep(10); // simulate work
   });
}
```
`workerThread(...)` calls `QThread::create(...)->start()` and also makes sure to clean-up the thread after itself. In the simplest example the whole function body is put inside the lambda.
