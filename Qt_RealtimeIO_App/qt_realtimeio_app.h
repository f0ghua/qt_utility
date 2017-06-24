/* 
Realtime Performance Test Application
Copyright (C) 2005,2007, James Gibbons

RTC part based on:
Real Time Clock Driver Test/Example Program
Copyright (C) 1996, Paul Gortmaker.

Released under the GNU General Public License, version 2,
included herein by reference.
*/

#ifndef QT_REALTIMEIO_APP_H
#define QT_REALTIMEIO_APP_H

#include <QMainWindow>
#include "ui_qt_realtimeio_app.h"
#include <QtCore>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QUdpSocket>
#include <QTimer>

static const int HMAX=300;	// maximum histogram coverage (0.0300 seconds)

#ifdef Q_WS_WIN

// define minimum Windows support for NT

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif

#include <windows.h>

//---------------------------------------------------------------------------
// high resolution timer object with inline functions
class THighResTimer
{
private:
  LARGE_INTEGER t0,t1,f;
  double et,last_dt;
  bool na;
protected:
public:
  THighResTimer(void)
  {
    QueryPerformanceFrequency(&f);
    na=f.QuadPart==0;
    HighResReset();
  }

  void HighResReset(void)   // reset timer
  {
    QueryPerformanceCounter(&t0);
    last_dt=0.0;
    et=0.0;
  }

  double HighResTime(void)  // elapsed time in seconds
  {
    double dt;

    if (na) return 0.0;

    QueryPerformanceCounter(&t1);
    dt=t1.QuadPart-t0.QuadPart;
    if (dt<0.0)
    {
      et=et+last_dt/f.QuadPart;
      t0=t1;
      last_dt=0.0;
      return et;
    }
    else
    {
      last_dt=dt;
      return et+dt/f.QuadPart;
    }
  }
};

#endif

#ifdef Q_WS_X11

//---------------------------------------------------------------------------
// high resolution timer object with inline functions
// actually only good for about 1 millisecond timing
class THighResTimer : public QObject
{
  Q_OBJECT

private:
	QTime t;
protected:
public:
  THighResTimer(void)
  {
    HighResReset();
  }

  void HighResReset(void)   // reset timer
  {
		t.start();
  }

  double HighResTime(void)  // elapsed time in seconds
  {
		return t.elapsed()/1000.0;
  }
};

#endif

//---------------------------------------------------------------------------
// realtime thread class
class RtThread : public QThread
{
  Q_OBJECT

public:
  RtThread(QObject *parent = 0);
  ~RtThread();

  // called by main GUI thread to start and stop "realtime" thread
  void startRtThread(QThread::Priority priority);
  void stopRtThread();

  // terminates thread
  bool abortRun;

  // setup values
  int clockMode;
  int clockRate;
  QString sendIpAddr;
  bool sendUDP;

  // statistics
  quint64 runCount;
  double runPeriod;
  double maxTime;
  double tt,t1,t2;
  quint64 hSec[HMAX];
  quint64 totalCount;

protected:
  // these are created and released in run() so "realtime" thread owns them
  QTimer *runTimer;
  QUdpSocket *udpSocket;
  int fd;	// linux only, RTC device file

  THighResTimer HRT;

  // run() is called by "realtime" thread
  void run();

  // linux only
  void close_rtc();

	protected slots:

  // these are called on timer and udp signals
  void runTimerTick();
  void udpRead();
};

//---------------------------------------------------------------------------
// realtime I/O app
class Qt_RealtimeIO_App : public QMainWindow
{
  Q_OBJECT

public:
  Qt_RealtimeIO_App(QWidget *parent = 0, Qt::WindowFlags  flags = 0);
  ~Qt_RealtimeIO_App();

private:
  Ui::Qt_RealtimeIO_AppClass ui;

  // the "realtime" thread
  RtThread rtThread;

  // timer used to run GUI display
  QTimer displayTimer;

  void setButtons(bool b);

  private slots:
    void runDisplay();
    void on_stopButton_clicked();
    void on_startButton_clicked();
    void on_sendUDP_stateChanged(int);
};

#endif // QT_REALTIMEIO_APP_H
