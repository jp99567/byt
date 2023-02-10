#pragma once

#include <vector>
#include <map>
#include <memory>

namespace can {

class ICanBus
{
public:
    virtual bool send() = 0;
    virtual ~ICanBus{}
};

using Id11 = uint16_t;
using Data = uint8_t[8];
struct Msg
{
    Id11 addr() const;
    Data data;
};

struct OutputMsg
{
    Id11 addr;
    bool needUpdate = false;
};

struct IInputItem
{
    virtual void update(const Data& data) = 0;
    virtual ~IInputItem(){}
};

struct IOutputItem
{
    virtual void update(OutputMsg& msg) = 0;
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

    void send()
    {
        busOutControl->update(idx, *this);
    }
};

struct DigOutItem : IOutputItem
{
    uint8_t mask;
    unsigned offset;
    void set()
    {

    }
    void update(OutputMsg& msg) override
    {

    }
};

class OutputControl : public IOutputControl
{
    void update(std::size_t idx, IOutputItem& item) override
    {
        auto& msg = outputs.at(idx);
        item.update(msg);
        msg.needUpdate = true;
        if(canbus.send(msg)){
            msg.needUpdate = false;
        }
    }

    void writeReady()
    {
        for(auto& msg : outputs)
        {
            if(msg.needUpdate)
            {
                if(canbus.send(msg)){
                    msg.needUpdate = false;
                }
                else{
                    return;
                }
            }
        }
    }


private:
    std::vector<OutputMsg> outputs;
    ICanBus& canbus;
};

class InputControl
{
public:
    void onRecvMsg(Msg& msg)
    {
        auto it = inputs.find(msg.addr());
        if( it != std::cbegin(inputs)) {
            it->second->update(msg.data);
        }
    }

private:
    std::map<Id11, std::unique_ptr<IInputItem>> inputs;
};

}
