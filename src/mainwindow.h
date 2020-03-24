#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void example1();
    void example2();
    void example3();
    void example4();
    void example5();
    void example6();
    void example7();
    void example8();
    void example9();

protected:
    void doExample3(int width);
};
#endif // MAINWINDOW_H
