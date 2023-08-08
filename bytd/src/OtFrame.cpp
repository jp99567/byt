#include "OtFrame.h"
#include <string>
#define OT_FRAME_ANALYZER
#ifdef OT_FRAME_ANALYZER
#include <spdlog/spdlog.h>
#include <iostream>
#endif

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

#ifdef OT_FRAME_ANALYZER
constexpr float floatFromf88(uint16_t v)
{
    return v / 256.0;
}

bool isMaster(opentherm::msg::type t)
{
    using opentherm::msg::type;
    for( auto mt : { type::Mrd, type::Mwr, type::Minvalid, type::Mreserved, type::Mwr2}){
        if(mt==t)
            return true;
    }
    return false;
}

int main(int argc, char* argv[])
{
    uint32_t uv;
    while(std::cin >> uv){
        opentherm::Frame f(uv);
        spdlog::info("{} {} {} ({})", opentherm::msg::msgToStr(f.getType()), f.getId(), f.getV(), floatFromf88(f.getV()));
    }
    return 0;
}
#endif
