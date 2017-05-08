#include "widget.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include "tcpconnection.h"
#include "bytrequestclient.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    connection = new QPushButton("connect");
    connection->setCheckable(true);
    audioPowerOn = new QPushButton("denon");
    audioCh1 = new QPushButton("ch1");
    audioCh2 = new QPushButton("ch2");
    connInfo = new QLabel("unknown");
    auto layout = new QVBoxLayout;
    layout->addWidget(connection);
    layout->addWidget(connInfo);
    layout->addWidget(audioPowerOn);
    layout->addWidget(audioCh1);
    layout->addWidget(audioCh2);
    setLayout(layout);
    bytConn = new TcpConnection;
    bytConn->moveToThread(&ioThread);
    connect(connection, &QPushButton::toggled, this, &Widget::onConnectionButtonChecked);
    connect(&ioThread, &QThread::finished, bytConn, &QObject::deleteLater);
    connect(this, &Widget::connectionReq, bytConn, &TcpConnection::connectTo);
    ioThread.start();
}

Widget::~Widget()
{
    ioThread.quit();
    ioThread.wait();
}

void Widget::onConnectionButtonChecked(bool on)
{
    if(on){
        emit connectionReq();
    }
}

