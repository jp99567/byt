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
    lbidich/packet.cpp \
    lbidich/io.cpp

HEADERS  += widget.h \
    bytrequestclient.h \
    tcpconnection.h \
    lbidich/packet.h \
    lbidich/iconnection.h \
    lbidich/channel.h \
    lbidich/byttransport.h \
    lbidich/io.h

INCLUDEPATH += ../../thrift-0.10.0/lib/cpp/src

unix:LIBS += -L../../thrift-0.10.0-x86_64/lib/
LIBS += -lthrift

CONFIG += c++14
