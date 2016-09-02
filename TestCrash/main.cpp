#include <QtGui/QApplication>
#include "dialog.h"
#include <QMessageBox>
#include "ccrashstack.h"
#include <QFile>
#include <QDebug>


long __stdcall   callback(_EXCEPTION_POINTERS*   excp)
{
    CCrashStack crashStack(excp);
    QString sCrashInfo = crashStack.GetExceptionInfo();
    QString sFileName = "testcrash.log";

    QFile file(sFileName);
    if (file.open(QIODevice::WriteOnly|QIODevice::Truncate))
    {
        file.write(sCrashInfo.toUtf8());
        file.close();
    }

    qDebug()<<"Error:\n"<<sCrashInfo;
    //MessageBox(0,L"Error",L"error",MB_OK);
    QMessageBox msgBox;
    msgBox.setText(QString::fromUtf8("亲，我死了，重新启动下吧！"));
    msgBox.exec();

    return   EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[])
{
    SetUnhandledExceptionFilter(callback);

    QApplication a(argc, argv);
    Dialog w;
    w.show();

    return a.exec();
}
