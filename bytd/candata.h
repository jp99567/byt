#pragma once

#include <vector>
#include <map>
#include <memory>

namespace can {

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
    void update(OutputMsg& msg) override
    {
        //ToDo
    }
};

class DataControl : public IOutputControl
{
    void update(std::size_t idx, IOutputItem& item) override
    {
        item.update(outputs.at(idx));
    }

    void onRecvMsg(Msg& msg)
    {
        auto it = inputs.find(msg.addr());
        if( it != std::cbegin(inputs)) {
            it->second->update(msg.data);
        }
    }

private:
    std::map<Id11, std::unique_ptr<IInputItem>> inputs;
    std::vector<OutputMsg> outputs;
};

}
