#pragma once

#include "iconnection.h"
#include <map>
#include "packet.h"

namespace lbidich
{

class IoBase : public IIo
{
public:
    IoBase();

    void setOnNewMsgCbk(ChannelId chId, OnNewDataCbk cbk) override
    {
        onNewMsgCallbacks[chId] = cbk;
    }

    bool onNewData(const uint8_t *ptr, const uint8_t *end);
    virtual bool onNewPacket(ChannelId ch, DataBuf msg);

protected:
    PacketIn packetIn;
    std::vector<uint8_t> dataWr;
    uint8_t dataRd[256];
    std::map<ChannelId, OnNewDataCbk> onNewMsgCallbacks;
};

}
