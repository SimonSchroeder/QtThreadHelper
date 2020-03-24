#include "mainwindow.h"

#include "thread_helper.h"

#include <QBoxLayout>
#include <QDebug>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *widget = new QWidget(this);
    this->setCentralWidget(widget);
    this->setStyleSheet("QPushButton { text-align: left; }");

    QVBoxLayout *layout = new QVBoxLayout(widget);
    QPushButton *buttonExample1 = new QPushButton(tr("Example 1: lambda"));
    QPushButton *buttonExample2 = new QPushButton(tr("Example 2: lambda with args"));
    QPushButton *buttonExample3 = new QPushButton(tr("Example 3: member function"));
    QPushButton *buttonExample4 = new QPushButton(tr("Example 4: GUI thread"));
    QPushButton *buttonExample5 = new QPushButton(tr("Example 5: blocking GUI thread"));
    QPushButton *buttonExample6 = new QPushButton(tr("Example 6: progress dialog"));
    QPushButton *buttonExample7 = new QPushButton(tr("Example 7: manual AUTODELETE"));
    QPushButton *buttonExample8 = new QPushButton(tr("Example 8: manual"));
    QPushButton *buttonExample9 = new QPushButton(tr("Example 9: worker thread"));

    layout->addWidget(buttonExample1);
    layout->addWidget(buttonExample2);
    layout->addWidget(buttonExample3);
    layout->addWidget(buttonExample4);
    layout->addWidget(buttonExample5);
    layout->addWidget(buttonExample6);
    layout->addWidget(buttonExample7);
    layout->addWidget(buttonExample8);
    layout->addWidget(buttonExample9);

    connect(buttonExample1, &QPushButton::clicked, this, &MainWindow::example1);
    connect(buttonExample2, &QPushButton::clicked, this, &MainWindow::example2);
    connect(buttonExample3, &QPushButton::clicked, this, &MainWindow::example3);
    connect(buttonExample4, &QPushButton::clicked, this, &MainWindow::example4);
    connect(buttonExample5, &QPushButton::clicked, this, &MainWindow::example5);
    connect(buttonExample6, &QPushButton::clicked, this, &MainWindow::example6);
    connect(buttonExample7, &QPushButton::clicked, this, &MainWindow::example7);
    connect(buttonExample8, &QPushButton::clicked, this, &MainWindow::example8);
    connect(buttonExample9, &QPushButton::clicked, this, &MainWindow::example9);
}

MainWindow::~MainWindow()
{
}

void MainWindow::example1()
{
    // offload work to new thread – GUI will stay responsive
    workerThread([this]()
    {
        // do some heavy computation in here...
        for(int i = 0; i < 10; ++i)
        {
            qInfo() << i << ":" << this->width();
            QThread::currentThread()->sleep(1);
        }
    });
}

void MainWindow::example2()
{
    // use lambda with arguments
    workerThread([this](int width, int height)
    {
        Q_UNUSED(height);

        // do some heavy computation in here...
        for(int i = 0; i < 10; ++i)
        {
            qInfo() << i << ":" << "old =" << width << "new =" << this->width();
            QThread::currentThread()->sleep(1);
        }
    }, this->width(), this->height());
}

void MainWindow::example3()
{
    // call member function directly
    workerThread(std::mem_fn(&MainWindow::doExample3), this, this->width());

    // we could have used a lambda insted
    //workerThread([this]() { this->doExample(this->width()); });
    // or
    //workerThread([this](int width) { this->doExample(width); }, this->width());
}

void MainWindow::doExample3(int width)
{
    // do some heavy computation in here...
    for(int i = 0; i < 10; ++i)
    {
        qInfo() << i << ":" << "old =" << width << "new =" << this->width();
        QThread::currentThread()->sleep(1);
    }
}

void MainWindow::example4()
{
    // modify GUI elements in the GUI thread
    workerThread([this]()
    {
        const QString oldTitle = this->windowTitle();

        for(int i = 0; i < 10; ++i)
        {
            //this->setWindowTitle(QString::number(i));     <-- this will crash because it is not in the GUI thread
            guiThread(std::mem_fn(&MainWindow::setWindowTitle), this, QString::number(i));
            QThread::currentThread()->sleep(1);
        }

        guiThread([this]()
        {
            QMessageBox::information(this, tr("Computation"), tr("Done."));
        });

        guiThread([this,oldTitle]() { this->setWindowTitle(oldTitle); });
    });
}

void MainWindow::example5()
{
    workerThread([this]()
    {
        const QString oldTitle = this->windowTitle();

        for(int i = 0; i < 10; ++i)
        {
            guiThread(std::mem_fn(&MainWindow::setWindowTitle), this, QString::number(i));
            QThread::currentThread()->sleep(1);
        }

        // blocking call: follow-up dialog will not pop up directly
        guiThread(WorkerThread::SYNC, [this]()
        {
            QMessageBox::information(this, tr("Computation"), tr("Done."));
        });

        // blocking call: window title will only be reset after this
        guiThread(WorkerThread::SYNC, [this](int result)    // test call with parameter
        {
            QMessageBox::information(this, tr("Result"), tr("Result is: %1").arg(result));
        }, 10);

        guiThread([this,oldTitle]() { this->setWindowTitle(oldTitle); });
    });
}

void MainWindow::example6()
{
    // non-blocking progress dialog
    workerThread([this]()
    {
        QProgressDialog *pd;
        // do this synchronously because otherwise we might update the progress too early
        guiThread(WorkerThread::SYNC, [this,&pd]() { pd = new QProgressDialog(this); });

        for(int i = 0; i < 100; ++i)
        {
            guiThread([pd,i]() { pd->setValue(i); });
            QThread::currentThread()->msleep(100);
        }

        guiThread(std::mem_fn(&QProgressDialog::setValue), pd, 100);

        // even the delete needs to be in the GUI thread...
        guiThread([pd]() { delete pd; });
    });
}

void MainWindow::example7()
{
    // manual setup of WorkerThread with AUTODELETE -> just use workerThread(...) instead
    WorkerThread *wt = new WorkerThread(WorkerThread::AUTODELETE);  // will call 'delete' in the end -> variable must be on the heap!
    wt->exec([]()
    {
        for(int i = 0; i < 10; ++i)
        {
            qInfo() << "i =" << i;
            QThread::currentThread()->sleep(1);
        }
    });
}

void MainWindow::example8()
{
    {
        // do your setup before the worker thread here
        qInfo() << "Setup block";
    }
    // manual setup of WorkerThread with AUTODELETE
    WorkerThread *wt = new WorkerThread(WorkerThread::DELETETHREAD);    // will call 'delete' in the end -> variable must be on the heap!
    connect(wt, &WorkerThread::done, [wt]() // register for clean-up
    {
        qInfo() << "Clean-up block";
        delete wt;                          // it is our turn to delete -> careful about object lifetime when using stack variable instead!!!
    });

    wt->exec([]()
    {
        for(int i = 0; i < 10; ++i)
        {
            qInfo() << "i =" << i;
            QThread::currentThread()->sleep(1);
        }
    });
}

class Example9Class
{
public:
    typedef QLabel MyCanvasClass;

    QDialog *dlg;
    MyCanvasClass *canvas;
    WorkerThread *drawingThread;
    int currentDrawingId = 0;
    bool interruptDrawing = false;

    Example9Class() : drawingThread(new WorkerThread())
    {
        // BEGIN: demo setup ---------------------------------------------------
        dlg = new QDialog();
        QVBoxLayout *layout = new QVBoxLayout(dlg);
        canvas = new MyCanvasClass();
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
        layout->addWidget(canvas);
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
        QObject::connect(dlg, &QDialog::accepted, drawingThread, &WorkerThread::quit);
        QObject::connect(drawingThread, &WorkerThread::done, dlg, &QDialog::deleteLater);
        QObject::connect(drawingThread, &WorkerThread::done, [this](){ delete this; });
        dlg->show();
        // END: demo setup -----------------------------------------------------
    }
    ~Example9Class()
    {
        delete drawingThread;
    }

    void draw()
    {
        interruptDrawing = true;
        ++currentDrawingId;

        const int myDrawingId = currentDrawingId;

        qInfo() << "queuing" << myDrawingId;

        drawingThread->exec([this,myDrawingId]()
        {
            // test if no other paint request has been queued since we were queued
            if(myDrawingId != currentDrawingId)
                return;

            qInfo() << "drawing" << myDrawingId;

            interruptDrawing = false;

            QPixmap *pixmap = new QPixmap(100,100);
            QPainter painter(pixmap);
            const int index = (myDrawingId) % 6;
            const int r = index <= 2 ? 255 : 0;
            const int g = index >= 2 && index <= 4 ? 255 : 0;
            const int b = index >= 4 || index == 0 ? 255 : 0;
            painter.setPen(QColor(r,g,b));
            for(int j = 0; j < 100; ++j)
            {
                // test if we were interrupted by the next paint request
                if(interruptDrawing)
                    break;

                if(j % 10 == 9)
                    qInfo() << "   line" << j+1;

                for(int i = 0; i < 100; ++i)
                {
                    painter.drawPoint(i, j);
                }

                QThread::currentThread()->msleep(20);  // drawing takes about 2 seconds...
            }
            painter.end(); // pixmap is only usable after painter.end()!!!

            if(!interruptDrawing)
            {
                guiThread([this,pixmap]()
                {
                    canvas->setPixmap(*pixmap);
                    dlg->update();
                    delete pixmap;
                });
            }
        });
    }
};

void MainWindow::example9()
{
    Example9Class *widget = new Example9Class();

    // simulate a series of update requests
    workerThread([widget]()   // use another worker thread to avoid blocking the GUI thread
    {
        // usually this would be triggered by user interaction
        widget->draw();     // red image
        QThread::currentThread()->sleep(3); // give the first draw call enough time

        widget->draw();     // yellow image
        QThread::currentThread()->sleep(1); // interrupt drawing somewhere in the middle

        widget->draw();     // green image
        widget->draw();     // cyan image   -> directly interrupt previous draw call
    });
}




















