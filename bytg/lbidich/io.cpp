#include "io.h"

namespace lbidich {

IoBase::IoBase()
    :onNewMsgCallbacks({ {ChannelId::down, [](DataBuf) {} },
                         {ChannelId::up, [](DataBuf) {}   } })
{}

bool IoBase::onNewData(const uint8_t* ptr, const uint8_t* end)
{
    do {
        ptr = packetIn.load(ptr, end-ptr);
        if(ptr) {
            auto chid = (ChannelId)packetIn.getHeader().chId;
            DataBuf msg = packetIn.getMsg();
            return onNewPacket(chid, std::move(msg) );
        }
        if(ptr == end)
            break;
    }
    while(ptr);

    return true;
}

bool IoBase::onNewPacket(ChannelId ch, DataBuf msg)
{
    onNewMsgCallbacks[ch](std::move(msg));
    return true;
}

bool IoBase::onClose()
{
    for(auto& ch : onNewMsgCallbacks)
    {
        ch.second({});
    }
}

}
