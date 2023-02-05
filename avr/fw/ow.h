#include "OwResponseCode_generated.h"

namespace ow {

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
#pragma pack(pop)
}
