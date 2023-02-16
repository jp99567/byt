#pragma once

#include <vector>
#include <map>
#include <memory>
#include <exception>
#include <linux/can.h>
#include <cstring>

#include "IIo.h"

namespace can {

class ICanBus
{
public:
    virtual bool send(const can_frame& msg) = 0;
    virtual ~ICanBus(){}
};

using Id = canid_t;
using Data = decltype(can_frame::data);

struct Frame
{
    Frame(Id id, std::size_t len)
    {
        frame.can_id = id;
        setSize(len);
        ::memset(frame.data, 0, len);
    }

    void setSize(std::size_t len)
    {
        if(len > sizeof(frame.data))
            throw std::out_of_range("can::Frame dlc invalid");
        frame.can_dlc = len;
    }

    Id addr() const
    {
        return frame.can_id;
    }
    can_frame frame;
};

struct OutputMsg
{
    OutputMsg(Id id, std::size_t len):msg(id, len){}
    bool needUpdate = false;
    Frame msg;
};

struct IInputItem
{
    virtual void update(const Data& data, std::size_t len) = 0;
    virtual ~IInputItem(){}
};

struct IOutputItem
{
    virtual void update(Data& data) = 0;
    virtual ~IOutputItem(){}
};

class IOutputControl
{
public:
    virtual void update(std::size_t idx, IOutputItem& item) = 0;
    virtual ~IOutputControl(){}
};

struct OutputItem : IOutputItem
{
    std::size_t idx;
    std::shared_ptr<IOutputControl> busOutControl;

    void send();
};

struct DigOutItem : OutputItem
{
    uint8_t mask;
    unsigned offset;
    bool value = false;

    void set(bool v);
    void update(Data& data) override;
};

class DigiInItem : public IInputItem
{
    uint8_t mask;
    unsigned offset;
    DigInput input;
    void update(const Data& data, std::size_t len) override;
};

class OutputControl : public IOutputControl, std::enable_shared_from_this<OutputControl>
{
    void update(std::size_t idx, IOutputItem& item) override;
    void writeReady();

    std::shared_ptr<OutputControl> getptr() {
        return shared_from_this();
    }
    [[nodiscard]] static std::shared_ptr<OutputControl> create(ICanBus& canbus) {
        return std::shared_ptr<OutputControl>(new OutputControl(canbus));
    }
private:
    OutputControl(ICanBus& canbus):canbus(canbus){}

    std::vector<OutputMsg> outputs;
    ICanBus& canbus;
};

class InputControl
{
public:
    void onRecvMsg(Frame& msg);

private:
    std::map<Id, std::unique_ptr<IInputItem>> inputs;
};

}

class CanDigiOut : public IDigiOut
{
    can::DigOutItem item;
public:
    operator bool() const override
    {
        return item.value;
    }
    bool operator()(bool value) override
    {
        item.set(value);
        return value;
    }
    can::DigOutItem& getCanItem(){return item;}
};

