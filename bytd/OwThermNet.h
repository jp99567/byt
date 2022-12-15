#pragma once

#include<memory>
#include "Pru.h"
#include "data.h"

namespace ow {

struct ThermScratchpad
{
    int16_t temperature;
    int8_t alarmH;
    int8_t alarmL;
    uint8_t conf;
    char reserved[3];
    uint8_t crc;
}__attribute__ ((__packed__));

class OwThermNet {
    enum class Cmd : uint8_t
    {
        READ_ROM = 0x33,
        CONVERT = 0x44,
        MATCH_ROM = 0x55,
        READ_SCRATCHPAD = 0xBE,
        SKIP_ROM = 0xCC,
        SEARCH = 0xF0,
    };

public:
    explicit OwThermNet(std::shared_ptr<Pru> pru);
    ~OwThermNet();

    std::vector<RomCode> search();
    bool convert();
    float measure(const RomCode &rc);
    bool read_rom(RomCode& rc);
    bool read_scratchpad(const RomCode& rc, ThermScratchpad& v);

private:
    bool presence();
    void write_bits(const void* data, std::size_t bitlen, bool strong_pwr=false);
    bool read_bits(std::size_t bitlen);
    void write_simple_cmd(Cmd cmd, bool strongpower=false);
    int search_triplet(bool branch_direction);

    std::shared_ptr<Pru> pru;
    std::shared_ptr<PruRxMsg> rxMsg;

    struct PruWriteMsg
    {
        struct {
          uint32_t code;
          uint32_t bitlen;
        } h;
        uint8_t data[24];
    };

    struct PruReadMsg
    {
        struct {
          uint32_t code;
        } h;
        uint8_t data[28];
    };

    union {
        PruWriteMsg wr;
        PruReadMsg rd; } msg;
};
}
