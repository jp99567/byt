#pragma once

#include <cstdint>

namespace opentherm
{
namespace msg
{
    enum class type
    {
        Mrd = 0b000'0000,
        Mwr = 0b001'0000,
        Minvalid = 0b010'0000,
        Mreserved = 0b011'0000,
        Srdack = 0b100'0000,
        Swrack = 0b101'0000,
        Sinvalid = 0b110'0000,
        Sunknown = 0b111'0000,
        Mwr2 = 0b001'1000
    };
}
}

namespace {

constexpr uint32_t calc_parity(unsigned v, unsigned pos);

constexpr uint32_t calc_parity(unsigned v, unsigned pos)
{
        if(pos==0)
                return v & 1;

        return ((v & (1<<pos)) ? 1 : 0)
        + calc_parity(v, pos-1);
}

constexpr uint32_t parity(uint32_t v)
{
    auto bitsnr = calc_parity(v, 30);
    constexpr uint32_t parity_bit_mask = (1<<31);
    return ( v & ~parity_bit_mask) | (( bitsnr & 1) ? parity_bit_mask : 0);
}

constexpr uint16_t float2f88(float v)
{
        return std::lround(v * 256);
}

constexpr float floatFromf88(uint16_t v)
{
        return v / 256.0;
}

struct Frame
{
    void setType(opentherm::msg::type type)
    {
        reinterpret_cast<uint8_t*>(&data)[0] = (uint8_t)type;
    }

    opentherm::msg::type getType() const
    {
        return (opentherm::msg::type)(reinterpret_cast<const uint8_t*>(&data)[0]);
    }

    void setId(int id)
    {
        reinterpret_cast<uint8_t*>(&data)[1] = (uint8_t)id;
    }

    int getId() const
    {
        return (int)(reinterpret_cast<const uint8_t*>(&data)[1]);
    }

    void setV(uint16_t v)
    {
        reinterpret_cast<uint16_t*>(&data)[1] = (uint16_t)v;
    }

    uint16_t getV() const
    {
        return (reinterpret_cast<const uint16_t*>(&data)[1]);
    }

    Frame(opentherm::msg::type type, int id, uint16_t val)
    {
        setType(type);
        setId(id);
        setV(val);
    }

    Frame(uint32_t data)
    :data(data){}

    bool isValid() const { return data != invalid; }

    static constexpr uint32_t invalid = 0x7FFFFFFF;
    uint32_t data = invalid;
};

}
