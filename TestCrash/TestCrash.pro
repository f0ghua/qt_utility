#-------------------------------------------------
#
# Project created by QtCreator 2013-09-30T10:28:08
#
#-------------------------------------------------

QT       += core gui

TARGET = TestCrash
TEMPLATE = app


SOURCES += main.cpp\
        dialog.cpp \
    ccrashstack.cpp

HEADERS  += dialog.h \
    ccrashstack.h

FORMS    += dialog.ui

QMAKE_CXXFLAGS_RELEASE += -g

QMAKE_CFLAGS_RELEASE += -g

QMAKE_LFLAGS_RELEASE = -mthreads -Wl
