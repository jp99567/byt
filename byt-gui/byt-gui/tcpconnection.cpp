#include "tcpconnection.h"

#include <QTcpSocket>
#include <QDebug>

TcpConnection::TcpConnection()
{
    socket = std::unique_ptr<QAbstractSocket>(new QTcpSocket(this));
    connect(socket.get(), &QAbstractSocket::stateChanged, this, &TcpConnection::stateChanged);
}

TcpConnection::~TcpConnection()
{
    qDebug() << "destroy" << "~TcpConnection";
}

void TcpConnection::connectTo() {
    socket->connectToHost("127.0.0.1", 1981);
}

void TcpConnection::stateChanged(QAbstractSocket::SocketState state)
{
    qDebug() << state;
}
