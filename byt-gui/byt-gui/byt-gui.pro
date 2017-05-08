#-------------------------------------------------
#
# Project created by QtCreator 2017-05-07T21:46:59
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = byt-gui
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp \
    tcpconnection.cpp

HEADERS  += widget.h \
    bytrequestclient.h \
    tcpconnection.h

LIBS += -lthriftc

INCLUDEPATH += /home/j/workspace/repo/bytd

CONFIG -= c++11
QMAKE_CXXFLAGS += -Werror -std=gnu++14
