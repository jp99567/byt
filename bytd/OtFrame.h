#pragma once

#include <cstdint>
#include <string_view>

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

    std::string_view msgToStr(type m);
}

struct Frame
{
    void setType(opentherm::msg::type type);

    opentherm::msg::type getType() const;

    void setId(unsigned id);

    int getId() const;

    void setV(uint16_t v);

    uint16_t getV() const;

    Frame(opentherm::msg::type type, int id, uint16_t val);

    Frame(uint32_t data)
    :data(data){}

    bool isValid() const { return data != invalid; }

    static constexpr uint32_t invalid = 0x7FFF7BAD;
    uint32_t data = invalid;
};

}

