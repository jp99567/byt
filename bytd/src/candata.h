#pragma once

#include "IIo.h"
#include "IPwmDev.h"
#include "IMqttPublisher.h"
#include "OwTempSensor.h"
#include "Sensorion.h"
#include "canbus.h"
#include <cstring>
#include <exception>
#include <linux/can.h>
#include <map>
#include <memory>
#include <vector>
#include <cmath>

namespace can {

using Id = canid_t;
using Data = decltype(can_frame::data);

struct Frame {
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

struct OutputMsg {
    OutputMsg(Id id, std::size_t len)
        : msg(id, len)
    {
    }
    bool needUpdate = false;
    Frame msg;
};

struct IInputItem {
    virtual void update(const Data& data, std::size_t len) = 0;
    virtual ~IInputItem() { }
};

struct IOutputItem {
    virtual void update(Data& data) = 0;
    virtual ~IOutputItem() { }
};

class IOutputControl {
public:
    virtual void update(std::size_t idx, IOutputItem& item) = 0;
    virtual ~IOutputControl() { }
};

struct OutputItem : IOutputItem {
    std::size_t idx;
    std::shared_ptr<IOutputControl> busOutControl;

    void send();
};

struct DigOutItem : OutputItem {
    uint8_t mask;
    unsigned offset;
    bool value = false;

    void set(bool v);
    void update(Data& data) override;
};

struct Pwm16Item : OutputItem {
    unsigned offset;
    uint16_t value = false;

    void set(uint16_t v);
    void update(Data& data) override;
};

struct DigiInItem : public IInputItem, DigInput {
    explicit DigiInItem(std::string name, uint8_t mask, unsigned offset)
        : DigInput({ name })
        , mask(mask)
        , offset(offset)
    {
    }
    DigiInItem(DigiInItem& other) = delete;
    uint8_t mask;
    unsigned offset;
    void update(const Data& data, std::size_t len) override;
};

struct OwTempItem : public IInputItem {
    explicit OwTempItem(std::string name, unsigned offset, IMqttPublisherSPtr mqtt, float factor)
        : offset(offset)
        , sens(name, mqtt, factor)
    {
    }
    OwTempItem(OwTempItem& other) = delete;
    unsigned offset;
    void update(const Data& data, std::size_t len) override;
    ow::Sensor sens;
};

class OutputControl : public IOutputControl {
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
class InputControl {
public:
    explicit InputControl(CanInputItemsMap&& inputsMap)
        : inputs(std::move(inputsMap))
    {
    }
    void onRecvMsg(const can_frame& msg);

private:
    CanInputItemsMap inputs;
};

template <typename Sensor>
struct SensorionItem : public IInputItem {
    explicit SensorionItem(std::string name, unsigned offset, IMqttPublisherSPtr mqtt)
        : offset(offset)
        , sens(name, mqtt)
    {
    }
    SensorionItem(SensorionItem& other) = delete;
    unsigned offset;
    void update(const Data& data, std::size_t len) override
    {
        int16_t owval = Sensorion::cInvalidValue;
        if(offset + sizeof(owval) <= len) {
            owval = *reinterpret_cast<const int16_t*>(&data[offset]);
            sens.setValue(owval);
        }
    }

    Sensor sens;
};
}

class CanDigiOut : public IDigiOut {
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
    can::DigOutItem& getCanItem() { return item; }
};

struct DaliConv
{
    const double top = 255;
    DaliConv(double top):top(top){}
    double conv(double vin) const
    {
        if(vin > 0){
            vin *= 2.54;
            double exp = 3 * (vin - 254)/253.0;
            return std::pow(10.0, exp) * top;
        }
        return 0;
    }
};

class CanPwm16Out : public IPwmDev {
    can::Pwm16Item item;
    const DaliConv conv;

public:
    explicit CanPwm16Out(uint16_t top):conv(top){}
    operator float() const override
    {
        return item.value;
    }
    float operator()(float value) override
    {
        if(value > IPwmDev::fullRange)
            value = IPwmDev::fullRange;
        else if(value < 0)
            value = 0;

        item.set(conv.conv(value));
        return value;
    }
    can::Pwm16Item& getCanItem() { return item; }
};
