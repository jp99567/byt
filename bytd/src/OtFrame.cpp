#include "OtFrame.h"

namespace opentherm
{

std::string_view msg::msgToStr(type m)
{
    switch(m){
    case type::Mrd: return "Mrd";
    case type::Mwr: return "Mwr";
    case type::Minvalid: return "Minvalid";
    case type::Mreserved: return "Mreserved";
    case type::Srdack: return "Srdack";
    case type::Swrack: return "Swrack";
    case type::Sinvalid: return "Sinvalid";
    case type::Sunknown: return "Sunknown";
    case type::Mwr2: return "Mwr2";
    default: return "---";
    }
}

void Frame::setType(msg::type type)
{
    data &= ~(0xFF << 24);
    data |= (uint8_t)type << 24;
}

msg::type Frame::getType() const
{
    uint8_t type = (data >> 24) & 0x7F;
    return (opentherm::msg::type)type;
}

void Frame::setId(unsigned id)
{
    data &= ~(0xFF << 16);
    data |= (id & 0xFF) << 16;
}

int Frame::getId() const
{
    uint8_t id = (data >> 16) & 0xFF;
    return id;
}

void Frame::setV(uint16_t v)
{
    data &= 0xFFFF0000;
    data |= v;
}

uint16_t Frame::getV() const
{
    return data & 0xFFFF;
}

Frame::Frame(msg::type type, int id, uint16_t val)
{
    setType(type);
    setId(id);
    setV(val);
}

}
