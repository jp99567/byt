#pragma once

#include <memory>
#include <queue>
#include <map>
#include <functional>
#include <condition_variable>
#include <QAbstractSocket>
#include "lbidich/packet.h"
#include "lbidich/channel.h"
#include "lbidich/byttransport.h"

namespace lbidich{

class IoBase : public IIo
{

};

}

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
