#include "widget.h"
#include "widget2.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
    widget2 w2;

    QObject::connect(&w2,&widget2::gotow,[&]()
    {
        w2.hide();
        w.show();
    });
    w2.show();
    return a.exec();
}
