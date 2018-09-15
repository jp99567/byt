
#pragma once

enum OwResponse {
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

enum eCommands
{
    eCmdHalt,
    eCmdOwInit,
    eCmdOwSearch,
    eCmdOwWrite,
    eCmdOwRead,
    eCmdOwWritePower
};
