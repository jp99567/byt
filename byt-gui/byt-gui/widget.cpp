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
    bytClient = new BytRequest(bytConn->getClientChannel());
    bytClient->moveToThread(&clientThread);
    connect(connection, &QPushButton::toggled, this, &Widget::onConnectionButtonChecked);
    connect(&ioThread, &QThread::finished, bytConn, &QObject::deleteLater);
    connect(&clientThread, &QThread::finished, bytClient, &QObject::deleteLater);
    connect(this, &Widget::connectionReq, bytConn, &TcpConnection::connectTo);
    connect(audioCh1, &QPushButton::clicked, bytClient, &BytRequest::c1);
    connect(audioCh2, &QPushButton::clicked, bytClient, &BytRequest::c2);
    ioThread.start();
    clientThread.start();
}

Widget::~Widget()
{
    clientThread.quit();
    clientThread.wait();
    ioThread.quit();
    ioThread.wait();
}

void Widget::onConnectionButtonChecked(bool on)
{
    if(on){
        emit connectionReq();
    }
}

