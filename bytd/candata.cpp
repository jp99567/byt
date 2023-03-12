#include "candata.h"
#include "../avr/fw/OwResponseCode_generated.h"

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

void InputControl::onRecvMsg(Frame &msg)
{
    auto it = inputs.find(msg.addr());
    if( it != std::cbegin(inputs)) {
        for(auto& item : it->second)
            item->update(msg.frame.data, msg.frame.can_dlc);
    }
}

void OutputControl::update(std::size_t idx, IOutputItem &item)
{
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
        value = data[offset] & mask;
        Changed.notify(value);
    }
}

void OwTempItem::update(const Data &data, std::size_t len)
{
    int16_t owval;
    if( offset+sizeof(owval) < len){
        owval = *reinterpret_cast<const int16_t*>(&data[offset]);
        auto newVal = std::numeric_limits<float>::quiet_NaN();
        if(owval != ow::cInvalidValue ){
            newVal = owval / factor;
        }
        if(value != newVal){
            value = newVal;
            Changed.notify(value);
        }
    }
}

}
