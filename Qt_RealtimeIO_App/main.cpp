#include <QApplication>
#include "qt_realtimeio_app.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Qt_RealtimeIO_App w;
    w.show();
    a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
    return a.exec();
}
