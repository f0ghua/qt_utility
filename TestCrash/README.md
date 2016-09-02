Qt程序crash信息的捕捉与跟踪

在用qt编写程序时经常会遇到崩溃问题，如果抓取不到crash堆栈信息就会对崩溃问题束手无策，只能对其进行复现，推断。

一般解决crash问题时有如下步骤：

1.从软件发行版本能跟获得debug信息，在不同平台下有不同的表现方式，目前只讨论qt-mingw方式，这种方式可以利用修改工程文件配置项编译时讲debug信息加入应用程序当中；当然这会增加发行版应用程序的体积。如果想体积变小可以strip应用程序。

2.获得crash堆栈信息

3.根据crash堆栈信息和1中的debug信息来查找软件崩溃的位置。

如何执行以上3步骤，下面我详细介绍如何操作;

步骤1：

在工程文件.pro中加入如下代码，生成可执行文件中就会带debug信息：
QMAKE_CXXFLAGS_RELEASE += -g
QMAKE_CFLAGS_RELEASE += -g
QMAKE_LFLAGS_RELEASE = -mthreads -Wl

前两行意识意思为在release版本中增加debug信息；第三行意思为release版本中去掉-s参数，这样就生成对应符号表，可以调试跟踪；

步骤2：
(注：目前只讨论windows平台，linux和mac暂不讨论；)
需要调用window平台系统api进行截取crash信息及获得crash堆栈。
首先在main函数中调用系统API SetUnhandledExceptionFilter，该函数有个设置回调函数，软件崩溃时会回调该系统函数，并传回崩溃地址信息等。

如何调用，请看如下代码：

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

注：CCrashStack 是我写的类，目的是调用系统API获取crash堆栈信息；（目前只针对windows平台）

步骤3:

通过qt命令行进入 执行命令：
objdump -S xxx.exe >aaa.asm

命令执行完成后，根据步骤2中获得的crash堆栈信息在aaa.asm中查找响应地址，即可得到崩溃具体位置。