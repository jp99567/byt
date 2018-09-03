#include "widget.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QProcessEnvironment>
#include <QLabel>
#include "tcpconnection.h"
#include "bytrequestclient.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    connection = new QPushButton(QProcessEnvironment::systemEnvironment().value("BYT_HOST", "jp99567.hopto.org"));
    connection->setCheckable(true);
    audioPowerOn = new QPushButton("denon");
    audioCh1 = new QPushButton("ch1");
    audioCh1->setFlat(false);
    audioCh1->setAutoFillBackground(true);
    audioCh2 = new QPushButton("ch2");
    connInfo = new QLabel("disconnected");
    auto layout = new QVBoxLayout;
    layout->addWidget(connection);
    layout->addWidget(connInfo);
    layout->addWidget(audioPowerOn);
    layout->addWidget(audioCh1);
    layout->addWidget(audioCh2);
    setLayout(layout);
    bytConn.reset(new TcpConnection);
    bytConn->moveToThread(&ioThread);
    bytClient.reset(new BytRequest(*bytConn));
    bytClient->moveToThread(&clientThread);
    connect(connection, &QPushButton::toggled, this, &Widget::onConnectionButtonChecked);
    connect(this, &Widget::connectionReq, bytConn.get(), &TcpConnection::connectTo);
    connect(audioCh1, &QPushButton::pressed, bytClient.get(), &BytRequest::c1Pressed);
    connect(audioCh1, &QPushButton::released, bytClient.get(), &BytRequest::c1Released);
    connect(audioCh2, &QPushButton::clicked, bytClient.get(), &BytRequest::c2);
    connect(bytConn.get(), &TcpConnection::connectionChanged, connInfo, &QLabel::setText);
    connect(bytClient.get(), &BytRequest::newVal, [this](float v){
       connInfo->setText(QString("VAL:%1").arg(v));
    });
    ioThread.start();
    clientThread.start();
}

Widget::~Widget()
{
    clientThread.quit();
    ioThread.quit();
    clientThread.wait();
    ioThread.wait();
    qDebug() << "~Widget";
}

void Widget::onConnectionButtonChecked(bool on)
{
    if(on){
        emit connectionReq(connection->text());
    }
}

