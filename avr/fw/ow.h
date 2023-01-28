
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

enum ResponseCode {
    eRspError,
    eOwPresenceOk,
    eOwBusFailure0,
    eOwBusFailure1,
    eOwNoPresence,
    eOwBusFailureTimeout,
    eOwReadBitsOk,
    eOwReadBitsFailure,
    eOwWriteBitsOk,
    eOwWriteBitsFailure,
	eOwSearchResult0,
	eOwSearchResult1,
	eOwSearchResult11,
	eOwSearchResult00,
};
}
