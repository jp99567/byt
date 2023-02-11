#include "candata.h"

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
        it->second->update(msg.frame.data, msg.frame.can_dlc);
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
        input.value = data[offset] & mask;
        input.Changed.notify(input.value);
    }
}

}
