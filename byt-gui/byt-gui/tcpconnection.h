#pragma once

#include <memory>
#include <queue>
#include <map>
#include <functional>
#include <condition_variable>
#include <QAbstractSocket>
#include <thrift/transport/TTransport.h>
#include "lbidich/packet.h"
#include "lbidich/channel.h"

using apache::thrift::transport::TTransportException;

class BytTransport : public apache::thrift::transport::TTransport
{
public:

    BytTransport(lbidich::IConnection& conn)
        :connection(conn)
    {}

    virtual bool isOpen()
    {   auto rv = connection.isOpen();
        qDebug() << "BytTransport::isOpen()" << rv;
        return rv;
    }

    virtual void open()
    {
        qDebug() << "BytTransport::open()";
        if(!connection.open())
            throw TTransportException(TTransportException::NOT_OPEN, "Cannot open base TTransport.");
    }

    virtual void close()
    {
        qDebug() << "BytTransport::close()";
        if(!connection.close())
            throw TTransportException(TTransportException::NOT_OPEN, "Cannot close base TTransport.");
    }

    virtual uint32_t read_virt(uint8_t* buf, uint32_t len)
    {
        qDebug() << "BytTransport::read() req:" << len;
       auto size = connection.recv(buf, len);
       qDebug() << "BytTransport::read()" << size << '/' << len;
       if(size == 0)
           throw TTransportException(TTransportException::END_OF_FILE);
       else if(size < 0)
           throw TTransportException(TTransportException::UNKNOWN);
       return size;
    }

    virtual void write_virt(const uint8_t* buf, uint32_t len)
    {
        qDebug() << "BytTransport::write()" << len;
      if(!connection.send(buf, len))
        throw TTransportException(TTransportException::UNKNOWN);
    }

private:
    lbidich::IConnection& connection;
};

class TcpConnection : public QObject, public lbidich::IIo
{
    Q_OBJECT
public:
    TcpConnection();

    ~TcpConnection();

    boost::shared_ptr<apache::thrift::transport::TTransport> getClientChannel();

    bool put(lbidich::ChannelId chId, const uint8_t* msg, unsigned len) override;

    void setOnNewMsgCbk(lbidich::ChannelId/* chId*/, onNewDataCbk /*cbk*/) override
    {
        //TODO
    }

public slots:
    void connectTo();

signals:
    void resultReady(const QString &result);
    void writeReq(lbidich::DataBuf data);

private:
    std::unique_ptr<QAbstractSocket> socket;
    lbidich::PacketIn packetIn;
    std::vector<uint8_t> dataWr;
    lbidich::Channel channelDown;
    lbidich::Channel channelUp;
    std::map<lbidich::ChannelId, boost::shared_ptr<apache::thrift::transport::TTransport>> transport;

private slots:
    void stateChanged(QAbstractSocket::SocketState state);
    void writeReqSlot(lbidich::DataBuf data);
};
