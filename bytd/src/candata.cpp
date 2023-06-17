#include "candata.h"
#include "avr/fw/OwResponseCode_generated.h"
#include "avr/fw/SvcProtocol_generated.h"
#include "Log.h"

namespace {
template<typename T>
void set_bits(T& dest_reg, const T mask, bool setNotClear)
{
    if(setNotClear){
        dest_reg |= mask;
    }
    else{
        dest_reg &= ~mask;
    }
}
}

namespace can {

constexpr Id broadcastSvcId = 0x1D867E00|(1<<31);

Frame mkMsgSetAllStageOperating()
{
    Frame msg(broadcastSvcId, 2);
    constexpr uint8_t stage3run = 0b11000000;
    msg.frame.data[0] = Svc::Protocol::CmdSetStage;
    msg.frame.data[1] = stage3run;
    return msg;
}

void InputControl::onRecvMsg(const can_frame& msg)
{
    auto it = inputs.find(msg.can_id);
    if( it != std::cend(inputs)) {
        for(auto& item : it->second)
            item->update(msg.data, msg.can_dlc);
    }
}

OutputControl::OutputControl(CanBus &bus):canbus(bus)
{
    canbus.setWrReadyCbk([this]{
        writeReady();
    });
}

void OutputControl::update(std::size_t idx, IOutputItem &item)
{
    LogDBG("OutputControl::update {}", idx);
    auto& outObj = outputs.at(idx);
    item.update(outObj.msg.frame.data);
    outObj.needUpdate = true;
    if(canbus.send(outObj.msg.frame)){
        outObj.needUpdate = false;
    }
}

void OutputControl::writeReady()
{
    for(auto& msg : outputs)
    {
        if(msg.needUpdate)
        {
            if(canbus.send(msg.msg.frame)){
                msg.needUpdate = false;
            }
            else{
                return;
            }
        }
    }
}

void OutputControl::setOutputs(std::vector<OutputMsg> outputs)
{
    this->outputs = std::move(outputs);
}

void DigOutItem::set(bool v)
{
    if(v != value){
        value = v;
        send();
    }
}

void DigOutItem::update(Data &data)
{
    set_bits(data[offset], mask, value);
}

void OutputItem::send()
{
    busOutControl->update(idx, *this);
}

void DigiInItem::update(const Data &data, std::size_t len)
{
    if( offset < len){
        bool newVal = data[offset] & mask;
        if(newVal != value){
            value = newVal;
            LogDBG("can::DigiInItem {} {}", name, value);
            Changed.notify(value);
        }
    }
}

void OwTempItem::update(const Data &data, std::size_t len)
{
    int16_t owval;
    if( offset+sizeof(owval) <= len){
        owval = *reinterpret_cast<const int16_t*>(&data[offset]);
        auto newVal = std::numeric_limits<float>::quiet_NaN();
        if(owval != ow::cInvalidValue ){
            newVal = owval / factor;
        }
        if(value != newVal){
            value = newVal;
            LogDBG("can::OwTempItem {} {}", name, value);
            Changed.notify(value);
            mqtt->publish(std::string("rb/stat/ow/").append(name), value);
        }
    }
}

}
