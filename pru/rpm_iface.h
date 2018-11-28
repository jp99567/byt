
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
};

enum Commands
{
    eCmdHalt,
    eCmdOwInit,
    eCmdOwSearch,
    eCmdOwWrite,
    eCmdOwRead,
    eCmdOwWritePower
};

#ifdef __cplusplus
}
#endif
