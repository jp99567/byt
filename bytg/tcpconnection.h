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

namespace lbidich
{

class IoBase : public IIo
{
public:
    IoBase();
protected:
    lbidich::PacketIn packetIn;
    std::vector<uint8_t> dataWr;
    uint8_t dataRd[256];
    lbidich::Channel channelDown;
    lbidich::Channel channelUp;
    std::map<lbidich::ChannelId, boost::shared_ptr<apache::thrift::transport::TTransport>> transport;
    std::map<lbidich::ChannelId, onNewDataCbk> onNewMsgCallbacks;

    bool onNewData(const uint8_t *ptr, const uint8_t *end);
    virtual bool onNewPacket(lbidich::ChannelId ch, lbidich::DataBuf msg);
};

}

class TcpConnection : public QObject, public lbidich::IoBase
{
    Q_OBJECT
public:
    TcpConnection();

    ~TcpConnection();

    boost::shared_ptr<apache::thrift::transport::TTransport> getClientChannel();

    bool put(lbidich::ChannelId chId, const uint8_t* msg, unsigned len) override;

    void setOnNewMsgCbk(lbidich::ChannelId chId, onNewDataCbk cbk) override
    {
        onNewMsgCallbacks[chId] = cbk;
    }

public slots:
    void connectTo();

signals:
    void resultReady(const QString &result);
    void writeReq(lbidich::DataBuf data);

private:
    std::unique_ptr<QAbstractSocket> socket;

private slots:
    void stateChanged(QAbstractSocket::SocketState state);
    void writeReqSlot(lbidich::DataBuf data);
    void readReadySlot();
};
