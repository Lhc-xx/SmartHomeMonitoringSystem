#include "widget.h"
<<<<<<< HEAD
#include "widget2.h"
=======
>>>>>>> dev-xgq

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
<<<<<<< HEAD
    widget2 w2;

    QObject::connect(&w2,&widget2::gotow,[&]()
    {
        w2.hide();
        w.show();
    });
    w2.show();
=======
    w.show();
>>>>>>> dev-xgq
    return a.exec();
}
