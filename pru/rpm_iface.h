
#pragma once

#ifdef __cplusplus
namespace pru {
#endif

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
	eOtFrameError,
	eOtBusError,
	eOtOk
};

enum Commands
{
    eCmdHalt,
    eCmdOwInit,
    eCmdOwSearchDir0,
	eCmdOwSearchDir1,
    eCmdOwWrite,
    eCmdOwRead,
    eCmdOwWritePower,
    eCmdOtTransmit
};

#ifdef __cplusplus
}
#endif
