#-------------------------------------------------
#
# Project created by QtCreator 2017-05-07T21:46:59
#
#-------------------------------------------------

QT += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = app

SOURCES += main.cpp\
        widget.cpp \
    tcpconnection.cpp\
    gen-req/BytRequest.cpp \
    lbidich/packet.cpp

HEADERS  += widget.h \
    bytrequestclient.h \
    tcpconnection.h \
    lbidich/packet.h \
    lbidich/iconnection.h \
    lbidich/channel.h \
    lbidich/byttransport.h

LIBS += -lthrift

INCLUDEPATH += /home/j/workspace/repo/bytd

CONFIG += c++14
