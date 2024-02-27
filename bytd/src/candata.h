#pragma once

#include <vector>
#include <map>
#include <memory>
#include <exception>
#include <linux/can.h>
#include <cstring>
#include "canbus.h"
#include "IIo.h"
#include "IMqttPublisher.h"
#include "OwTempSensor.h"

namespace can {

using Id = canid_t;
using Data = decltype(can_frame::data);

struct Frame
{
    explicit Frame(Id id, std::size_t len)
    {
        frame.can_id = id;
        setSize(len);
        ::memset(frame.data, 0, len);
    }

    explicit Frame(const can_frame& other)
    {
        frame.can_id = other.can_id;
        frame.can_dlc = other.can_dlc;
        std::copy_n(std::cbegin(other.data), frame.can_dlc, std::begin(frame.data));
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

Frame mkMsgSetAllStageOperating();

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

struct DigiInItem : public IInputItem, DigInput
{
    explicit DigiInItem(std::string name, uint8_t mask, unsigned offset): DigInput({name}), mask(mask), offset(offset){}
    DigiInItem(DigiInItem& other) = delete;
    uint8_t mask;
    unsigned offset;
    void update(const Data& data, std::size_t len) override;
};

std::unique_ptr<IInputItem> createSensorion(std::string ToDo,
                                            std::string name,
                                            unsigned offset,
                                            IMqttPublisherSPtr mqtt);

struct OwTempItem : public IInputItem
{
    explicit OwTempItem(std::string name, unsigned offset, IMqttPublisherSPtr mqtt, float factor)
        : offset(offset)
        , sens(name, mqtt, factor)
    {}
    OwTempItem(OwTempItem& other) = delete;
    unsigned offset;
    void update(const Data& data, std::size_t len) override;
    ow::Sensor sens;
};

class OutputControl : public IOutputControl
{
public:
    explicit OutputControl(CanBus& bus);
    void update(std::size_t idx, IOutputItem& item) override;
    void writeReady();
    void setOutputs(std::vector<OutputMsg> outputs);
private:

    std::vector<OutputMsg> outputs;
    CanBus& canbus;
};

using CanInputItemsMap = std::map<Id, std::vector<std::unique_ptr<IInputItem>>>;
class InputControl
{
public:
    explicit InputControl(CanInputItemsMap&& inputsMap):inputs(std::move(inputsMap)){}
    void onRecvMsg(const can_frame& msg);

private:
    CanInputItemsMap inputs;
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
