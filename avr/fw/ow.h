#include "OwResponseCode_generated.h"

namespace ow {

enum class Cmd : uint8_t
{
    READ_ROM = 0x33,
    CONVERT = 0x44,
    MATCH_ROM = 0x55,
    READ_SCRATCHPAD = 0xBE,
    SKIP_ROM = 0xCC,
    SEARCH = 0xF0,
};

#pragma pack(push, 1)
struct RomCode
{
	uint8_t family = 0;
	uint8_t serial[6] = {0};
};

struct RomData : RomCode
{
	uint8_t crc = 0;
};

struct ThermScratchpad
{
    int16_t temperature;
    int8_t alarmH;
    int8_t alarmL;
    uint8_t conf;
    char reserved[3];
    uint8_t crc;
};

#pragma pack(pop)
}
