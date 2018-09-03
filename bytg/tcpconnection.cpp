#include "tcpconnection.h"

#include <QTcpSocket>
#include <QDebug>
#include <QMetaEnum>

using lbidich::ChannelId;

TcpConnection::TcpConnection()
{
    socket = new QTcpSocket(this);
    connect(socket, &QAbstractSocket::stateChanged, this, &TcpConnection::stateChanged);
    connect(this, &TcpConnection::writeReq, this, &TcpConnection::writeReqSlot, Qt::ConnectionType::QueuedConnection);
    connect(socket, &QAbstractSocket::readyRead, this, &TcpConnection::readReadySlot);
    connect(socket, &QAbstractSocket::connected, [this]{
        io = Io::create(*this);
        writeReqSlot(lbidich::packMsg2((uint8_t)ChannelId::auth, "Na Straz!"));
    });
}

void TcpConnection::writeReqSlot(lbidich::DataBuf data)
{
    if(!io)
        return;

    qDebug() << "writeReqSlot" << data.size();
        auto& dataWr = io->getDataWr();
        if(dataWr.size() > 0){
            std::copy(std::begin(data), std::end(data), std::back_inserter(dataWr));
            return;
        }

        auto written = socket->write((const char*)data.data(), data.size());
        qDebug() << "socket->write" << written;

        if(written == -1)
        {
            qDebug() << socket->errorString();
            io->onClose();
            io = nullptr;
            return;
        }

        if(written < (qint64)data.size())
        {
            std::copy(std::begin(data)+written, std::end(data), std::back_inserter(dataWr));
        }
        return;
}

void TcpConnection::readReadySlot()
{
    if(!io)
        return;
    
    qint64 readSize = 0;
    do{
        qDebug() << "socket read blocking...";
        readSize = socket->read(dataRd, sizeof(dataRd));

        if(readSize <= 0)
        {
            qDebug() << "QAbstractSocket::read" << socket->errorString();
            //TODO check
            break;
        }

        qDebug() << "socket read" << readSize;
        auto ptr = (const uint8_t*)dataRd;
        auto end = ptr + readSize;

        if(!io->onNewData(ptr, end))
        {
            qDebug() << "read: invalid data";
        }
    }
    while(readSize == sizeof(dataRd));
}

TcpConnection::~TcpConnection()
{
    qDebug() << "destroy" << "~TcpConnection";
}

std::shared_ptr<apache::thrift::transport::TTransport> TcpConnection::getClientChannel()
{
    if(!io)
        return nullptr;

    auto channel = std::make_unique<lbidich::Channel>(ChannelId::down, io);
    return std::shared_ptr<apache::thrift::transport::TTransport>(
                new BytTransport(std::move(channel)));
}

bool TcpConnection::put(lbidich::DataBuf msg)
{
    if(!socket->isValid())
        return false;
    
    emit writeReq(std::move(msg));
    return true;
}

void TcpConnection::connectTo(QString host)
{
    qDebug() << "connect to" << host;
    socket->connectToHost(host, 1981);
}

void TcpConnection::stateChanged(QAbstractSocket::SocketState state)
{
    qDebug() << state;
    QMetaEnum metaEnum = QMetaEnum::fromType<QAbstractSocket::SocketState>();
    emit connectionChanged(metaEnum.valueToKey(state));
}

bool Io::put(lbidich::ChannelId chId, const uint8_t *msg, unsigned len)
{
    return tcp.put(lbidich::packMsg((uint8_t)chId, msg, len));
}

bool Io::onNewPacket(lbidich::ChannelId ch, lbidich::DataBuf msg)
{
    switch(ch)
    {
    case lbidich::ChannelId::auth:
        break;
    case lbidich::ChannelId::down:
    case lbidich::ChannelId::up:
        return lbidich::IoBase::onNewPacket(ch, std::move(msg));
    default:
        return false;
    }

    return true;
}
