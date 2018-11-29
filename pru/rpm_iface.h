
#pragma once

#ifdef __cplusplus
namespace pru {
#endif

enum OwResponse {
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

enum Commands
{
    eCmdHalt,
    eCmdOwInit,
    eCmdOwSearchDir0,
	eCmdOwSearchDir1,
    eCmdOwWrite,
    eCmdOwRead,
    eCmdOwWritePower
};

#ifdef __cplusplus
}
#endif
