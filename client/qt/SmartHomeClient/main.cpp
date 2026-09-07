#include "MainWindow.h"

#include <QApplication>

// Qt 客户端程序入口：创建应用对象并显示主窗口。
int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    MainWindow mainWindow;
    mainWindow.show();

    return application.exec();
}
