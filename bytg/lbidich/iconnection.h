#pragma once
#include "packet.h"
#include <functional>

namespace lbidich {

enum class ChannelId
{
    auth,
    up,
    down,
    _count
};

class IIo
{
public:
    using OnNewDataCbk = std::function<void(DataBuf msg)>;
    virtual bool put(ChannelId chId, const uint8_t* msg, unsigned len) = 0;
    virtual void setOnNewMsgCbk(ChannelId chId, OnNewDataCbk cbk) = 0;
    virtual ~IIo() {}
};

class IConnection
{
public:
    virtual bool isOpen() const = 0;
    virtual bool send(const uint8_t* data, unsigned size) = 0;
    virtual int recv(uint8_t* data, unsigned size) = 0;
    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual ~IConnection() {}
};

}
