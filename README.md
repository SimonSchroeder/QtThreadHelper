# QtThreadHelper
Some QThread helper classes and functions for specific use cases (single header-only library, depends on Qt)

## TL;DR
First I give a short overview how to use this header-only library. It does __not__ make sense to use it as a general replacement for other ways of using threads. For infos when it makes sense read on further below.
### What you need
Copy `thread_helper.h` from `src` and add it to your project. Make sure that your compiler has C++17 support turned on (tested with VS2019). E.g. if you are using a QMake project file add the line
```
CONFIG += c++17
```

### How to use
1. Create a single-shot temporary thread by calling `workerThread([](){ /* do stuff */ });`.
1. Queue function calls into the GUI thread (when manipulating GUI elements) from another thread by calling `guiThread([]() { /* call function on widget */ });`.
1. Create a permanent worker thread object `WorkerThread *wt = new WorkerThread()` with its own event queue and continuously queue function calls: `wt->exec([]() { /* do stuff */ })`.
1. Easily use function arguments in all of the above, e.g. `workerThread([](int i) { /* do stuff */ }, 1);`.
1. Use member functions instead of lambdas in any of the above, e.g. `guiThread(std::mem_fn(&MainWindow::setWindowTitle), this, "Title");`.
