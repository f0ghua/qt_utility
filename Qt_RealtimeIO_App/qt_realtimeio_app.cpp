/* 
Realtime Performance Test Application
Copyright (C) 2005,2007, James Gibbons

RTC part based on:
Real Time Clock Driver Test/Example Program
Copyright (C) 1996, Paul Gortmaker.

Released under the GNU General Public License, version 2,
included herein by reference.
*/

#include "qt_realtimeio_app.h"
#include <QTextStream>
//#include <qDebug>

#ifdef Q_WS_X11
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#endif

Qt_RealtimeIO_App::Qt_RealtimeIO_App(QWidget *parent, Qt::WindowFlags flags)
: QMainWindow(parent, flags)
{
	ui.setupUi(this);

	// setup the controls
	ui.clockMode->clear();
	ui.clockMode->addItem("QTimer Event",QVariant());   // this mode runs on QTimer

#ifdef Q_WS_WIN
	ui.clockMode->addItem("Win32 Periodic",QVariant()); // this mode uses Win32 calls
#endif

#ifdef Q_WS_X11
	ui.clockMode->addItem("Qt msleep",QVariant());	// this mode uses msleep calls
	ui.clockMode->addItem("RTC wait",QVariant());		// this mode uses RTC waits
#endif

	ui.threadLevel->clear();
	ui.threadLevel->addItem("Idle",QVariant());
	ui.threadLevel->addItem("Lowest",QVariant());
	ui.threadLevel->addItem("Low",QVariant());
	ui.threadLevel->addItem("Normal",QVariant());
	ui.threadLevel->addItem("High",QVariant());
	ui.threadLevel->addItem("Highest",QVariant());
	ui.threadLevel->addItem("Time Critical",QVariant());
	ui.threadLevel->setCurrentIndex(6);
	ui.processLevel->clear();
	ui.processLevel->addItem("Idle",QVariant());
	ui.processLevel->addItem("Normal",QVariant());
	ui.processLevel->addItem("High",QVariant());
	ui.processLevel->addItem("Realtime",QVariant());
	ui.processLevel->setCurrentIndex(3);
	ui.textEdit->clear();

	// connect GUI display to timer
	connect(&displayTimer,SIGNAL(timeout()),this,SLOT(runDisplay()));

	ui.textEdit->clear();

#ifdef Q_WS_X11
	int EC;
	QString s;
	QTextStream ts(&s);

	// lock all current and future memory in RAM
	EC = mlockall( MCL_CURRENT + MCL_FUTURE );
	if ( EC != 0 )
	{
		s.clear();
		ts << "mlockall error " << EC << endl;
		ui.textEdit->insertPlainText(s);
	}
#endif
}

Qt_RealtimeIO_App::~Qt_RealtimeIO_App()
{
}

RtThread::RtThread(QObject* parent) : QThread(parent)
{
}

RtThread::~RtThread()
{
	stopRtThread(); // make sure we stop when app exits
}

void RtThread::startRtThread(QThread::Priority priority)
{
	//qDebug() << "startRtThread on thread " << GetCurrentThreadId();

	// make sure thread is not already running
	if (isRunning()) stopRtThread();

	// reset control variables
	runTimer=NULL;
	udpSocket=NULL;
	fd=-1;
	abortRun=false;
	runCount=0;
	runPeriod=0.0;
	maxTime=0.0;
	tt=t1=t2=0;
	for (int i=0;i<HMAX;i++) hSec[i]=0;
	totalCount=0;

	// start the "realtime" thread
	start(priority);
}

void RtThread::stopRtThread()
{
	// only stop if running
	if (isRunning())
	{
		//qDebug() << "stopRtThread on thread " << GetCurrentThreadId();

		// use proper method to stop thread
		switch (clockMode) {
			case 0:
				QThread::quit();  // if using event loop, send quit signal
				break;
			case 1:
			case 2:
				abortRun=true;    // if using wait loop, simply set flag
				break;
		}

		// wait 5 seconds for thread to stop
		if (!wait(5000))
		{
			// terminate it if it failed to stop
			terminate();
			wait(1000);
		}
	}
}

void RtThread::run()
{
	//qDebug() << "Thread run() on thread " << GetCurrentThreadId();

#ifdef Q_WS_X11
	sched_param sp;
	int min_pr, max_pr;
	int EC;

	// get real-time priority min/max
	max_pr = sched_get_priority_max( SCHED_FIFO );
	min_pr = sched_get_priority_min( SCHED_FIFO );

	// bump us up to maximum priority
	sp.sched_priority = max_pr;
	EC = pthread_setschedparam( pthread_self(), SCHED_FIFO, &sp );
	if ( EC != 0 )
	{
		exit(EC);
		return;
	}
#endif

	// create a UDP socket on the "realtime" thread
	udpSocket=new QUdpSocket();
	if (!udpSocket->bind(1667))
	{
		// exit with error if it doesn't bind
		udpSocket->close();
		delete udpSocket; udpSocket=NULL;
		exit(1);
	}

	// setup high res timer
	HRT.HighResReset();
	t1=HRT.HighResTime();

#ifdef Q_WS_WIN
	// used for Win32 mode timer
	HANDLE hTE=NULL;
#endif

	switch (clockMode) {

		// use QTimer and events
		case 0:
			{
				// this mode uses a QTimer and event loop to run
				runTimer=new QTimer();

				// connect the slots
				connect(runTimer,SIGNAL(timeout()),this,SLOT(runTimerTick()));
				connect(udpSocket,SIGNAL(readyRead()),this,SLOT(udpRead()));

				// don't use clock if period is zero, allows only UDP packet input testing
				if (clockRate>0) runTimer->start(clockRate);

				// run the event loop
				exec();

				// stop and release the QTimer
				if (clockRate>0) runTimer->stop();

				// disable this statement to cause a crash
				// the extra timer thread created by Qt keeps sending events and this
				// allows the extra thread to finish before this one goes away
				this->msleep(500);

				// disconnect the slots
				runTimer->disconnect(this);
				udpSocket->disconnect(this);

				delete runTimer; runTimer=NULL;
			}
			break;

			// run using a wait call, Win32 periodic timer or Qt msleep
		case 1:
			// use blocking udp mode if clock is zero
			if (clockRate<=0)
			{
				// start our blocking loop, wait for abort flag
				while (!abortRun)
				{
					if (udpSocket->waitForReadyRead(100))
					{
						// input is waiting, read it
						while (udpSocket->hasPendingDatagrams())
							udpRead();
					}
				}
			}
			else
			{

#ifdef Q_WS_WIN
				// setup waitable Win32 periodic timer
				hTE=(HANDLE)CreateWaitableTimer(NULL,false,NULL);
				if (hTE!=NULL)
				{
					LARGE_INTEGER tLT;
					tLT.QuadPart=0;
					if (!SetWaitableTimer(hTE,&tLT,clockRate,NULL,NULL,true))
					{
						CloseHandle(hTE);
						hTE=NULL;
					}
				}
#endif

				// wait for 10 ticks for stable timing
				for (int i=0;i<10;i++)
				{
#ifdef Q_WS_WIN
					if (hTE==NULL) Sleep(clockRate);
					else WaitForSingleObject(hTE,clockRate*10);
#else
					this->msleep(clockRate);
#endif
				}

				// start our timing loop, wait for abort flag
				while (!abortRun)
				{
#ifdef Q_WS_WIN
					if (hTE==NULL) Sleep(clockRate);
					else WaitForSingleObject(hTE,clockRate*10);
#else
					this->msleep(clockRate);
#endif

					// run timer tick after wait
					runTimerTick();
				}

#ifdef Q_WS_WIN
				// shut down Win32 timer
				if (hTE!=NULL) CloseHandle(hTE);
#endif
			}
			break;

			// special Linux RTC wait loop, not supported on some kernels
		case 2:
#ifdef Q_WS_X11
			// don't run if clock is zero
			if (clockRate<=0) break;

			// block for local variables
			{
				// adjust clock rate to power of 2
				if (clockRate>=64) clockRate=64;
				else if (clockRate>=32) clockRate=32;
				else if (clockRate>=16) clockRate=16;
				else if (clockRate>=8) clockRate=8;
				else if (clockRate>=4) clockRate=4;
				else if (clockRate>=2) clockRate=2;
				else if (clockRate>=1) clockRate=1;

				// open the RTC
				fd = open( "/dev/rtc", O_RDONLY );
				if ( fd == -1 )
				{
					abortRun=true;
					break;
				}

				unsigned long freq = 1024/clockRate;	// use 1K frequency
				unsigned long data;

				// set the frequency
				int retval = ioctl( fd, RTC_IRQP_SET, freq );
				if ( retval == -1 )
				{
					close_rtc();
					abortRun=true;
					break;
				}

				// Enable periodic interrupts
				retval = ioctl( fd, RTC_PIE_ON, 0 );
				if ( retval == -1 )
				{
					close_rtc();
					abortRun=true;
					break;
				}

				// start our timing loop, wait for abort flag
				while (!abortRun)
				{
					// This blocks until RTC interrupt
					retval = read( fd, &data, sizeof(data) );
					if ( retval == -1 )
					{
						abortRun=true;
					}
					else
					{
						// run timer tick after wait
						runTimerTick();
					}
				}

				close_rtc();
			}
#endif
			break;
	}

	// delete socket
	udpSocket->close();
	delete udpSocket; udpSocket=NULL;

	// "realtime" thread now terminates
}

void RtThread::close_rtc()
{
#ifdef Q_WS_X11
	if (fd<0) return;

	// Disable periodic interrupts
	int retval = ioctl( fd, RTC_PIE_OFF, 0 );
	if ( retval == -1 )
	{
	}

	// close the RTC
	close( fd );

	fd=-1;
#endif
}

void RtThread::runTimerTick()
{
	// this method does the statistics and outputs UDP packets for testing

	// use high resolution timer to check period
	t2=HRT.HighResTime();
	double dt=t2-t1;
	tt+=dt;
	t1=t2;

	// store period in histogram so we can see how accurate it is
	int h=dt*10000.0+0.5;
	if (h<0) h=0;
	if (h>=HMAX)
	{
		h=HMAX-1;
		if (dt>maxTime) maxTime=dt;
	}
	if (totalCount>10) hSec[h]++;

	// calculate average period every 100 ticks
	runCount++;
	if (runCount>=100)
	{
		runPeriod=tt/runCount;
		tt=0.0;
		runCount=0;
	}
	totalCount++;

	// only debug print on first 10 ticks
	//if (totalCount<10) qDebug() << "runTimerTick on thread " << GetCurrentThreadId();

	// send out a UDP packet if checkbox is set and socket was created
	if (sendUDP && (udpSocket!=NULL))
	{
		char Buf[64];
		QHostAddress hadr(sendIpAddr);
		udpSocket->writeDatagram(Buf,64,hadr,1667);
	}
}

void RtThread::udpRead()
{
	// this method is entered on a udp read signal

	// perform statistics
	// if clock is set to zero, only udp packets will provide statistics
	t2=HRT.HighResTime();
	double dt=t2-t1;
	tt+=dt;
	t1=t2;

	int h=dt*10000.0+0.5;
	if (h<0) h=0;
	if (h>=HMAX)
	{
		h=HMAX-1;
		if (dt>maxTime) maxTime=dt;
	}
	if (runCount>10) hSec[h]++;

	runCount++;
	if (runCount>=100)
	{
		runPeriod=tt/runCount;
		tt=0.0;
		runCount=0;
	}
	totalCount++;

	// only debug print first 10 packets
	//if (totalCount<10) qDebug() << "udpRead on thread " << GetCurrentThreadId();

	// input all pending packets if UDP socket created
	if ((udpSocket!=NULL) && udpSocket->hasPendingDatagrams())
	{
		qint64 nbytes;
		while ((nbytes=udpSocket->pendingDatagramSize())>0)
		{
			char *c=new char[nbytes];
			qint64 nb=udpSocket->readDatagram(c,nbytes);
			delete[] c;
		}
	}
}

void Qt_RealtimeIO_App::runDisplay()
{
	// GUI timer is used to display "realtime" thread statistics
	if (rtThread.isRunning())
	{
		QString s;
		QTextStream ts(&s);
		ts.setFieldWidth(10);
		ts.setRealNumberPrecision(4);
		ts.setRealNumberNotation(QTextStream::FixedNotation);
		ts << rtThread.runPeriod;
		ui.actualRate->setText(s);

		quint64 Total=0;

		ui.textEdit->clear();

		s.clear();
		ts << "Seconds" << "Count" << "%" << endl;
		ui.textEdit->insertPlainText(s);

		for (int i=0;i<HMAX;i++) Total+=rtThread.hSec[i];
		if (Total<=0) Total=1;


		for (int i=0;i<HMAX;i++)
		{
			double t=i*0.0001;
			quint64 c=rtThread.hSec[i];
			if (i==HMAX-1) t=rtThread.maxTime;

			if (c>0)
			{
				s.clear();
				ts << t << c << c*100.0/Total;
				if (i==HMAX-1) ts << " max";
				ts << endl;
				ui.textEdit->insertPlainText(s);
			}
		}
		ui.textEdit->moveCursor(QTextCursor::End);
	}
}

void Qt_RealtimeIO_App::on_sendUDP_stateChanged(int)
{
	rtThread.sendUDP=ui.sendUDP->isChecked();
}

void Qt_RealtimeIO_App::setButtons(bool b)
{
	// set ability to change controls and buttons
	ui.startButton->setEnabled(b);
	ui.clockMode->setEnabled(b);
	ui.sendIpAddr->setEnabled(b);
	ui.clockRate->setEnabled(b);
	ui.processLevel->setEnabled(b);
	ui.threadLevel->setEnabled(b);
	ui.stopButton->setEnabled(!b);
}

void Qt_RealtimeIO_App::on_startButton_clicked()
{
	// disable startup buttons
	setButtons(false);

	//qDebug() << "Starting on thread " << GetCurrentThreadId();

	// copy setup values to thread
	rtThread.clockMode=ui.clockMode->currentIndex();
	rtThread.clockRate=ui.clockRate->text().toInt();
	rtThread.sendIpAddr=ui.sendIpAddr->text();
	rtThread.sendUDP=ui.sendUDP->isChecked();

	QThread::Priority priority=QThread::InheritPriority;

	// set thread priority
	// only works well on Win32, Linux uses special calls to set realtime

	switch (ui.threadLevel->currentIndex()) {
		case 0:
			priority=QThread::IdlePriority;
			break;
		case 1:
			priority=QThread::LowestPriority;
			break;
		case 2:
			priority=QThread::LowPriority;
			break;
		case 3:
			priority=QThread::NormalPriority;
			break;
		case 4:
			priority=QThread::HighPriority;
			break;
		case 5:
			priority=QThread::HighestPriority;
			break;
		case 6:
			priority=QThread::TimeCriticalPriority;
			break;
	}

#ifdef Q_WS_WIN
	// Windows specific process level
	switch (ui.processLevel->currentIndex()) {
		case 0: SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS); break;
		case 1: SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS); break;
		case 2: SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS); break;
		case 3: SetPriorityClass(GetCurrentProcess(),REALTIME_PRIORITY_CLASS); break;
	}

	// GUI thread runs at normal
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);

	// use multi-media call to speed up clock
	if ((rtThread.clockMode==1) && (rtThread.clockRate<10)) 
		timeBeginPeriod(rtThread.clockRate);
#endif

	// move event processing for thread onto thread
	// this makes events occur on the thread instead of main window event loop
	rtThread.moveToThread(&rtThread);

	// start thread
	rtThread.startRtThread(priority);

	// start display
	ui.textEdit->clear();
	displayTimer.start(100);
}

void Qt_RealtimeIO_App::on_stopButton_clicked()
{
	// stop display
	displayTimer.stop();

	// stop thread
	rtThread.stopRtThread();

#ifdef Q_WS_WIN
	// reset multi-media clock
	if ((rtThread.clockMode==1) && (rtThread.clockRate<10)) 
		timeEndPeriod(rtThread.clockRate);

	// back to normal priority
	SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);
#endif

	// reset buttons for normal use
	setButtons(true);
}
