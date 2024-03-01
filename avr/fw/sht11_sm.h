#pragma once
#include <stdint.h>

namespace sht11 {

enum class State {
    Idle,
    Error,
    Finising,
    InitTransfer,
    InitTransferDataPull,
    InitTransferLowPulseClk0,
    InitTransferLowPulseClk1,
    InitTransferDataRelease,
    InitTransferClk0,
    Send8bit,
    Send8bitHiPulseClk1,
    Send8bitHiPulseClk0,
    Send8bitReleaseBAck,
    Send8bitHiPluseAckClk1,
    Send8bitHiPluseAckClk0,
    Send8bitWaitMeas,
    Recv8bit,
    Recv8HiPulseClk1,
    Recv8HiPulseClk0,
    Recv8bitSendAck,
    Recv8bitSendAckClk1,
    Recv8bitSendAckClk0,
    Recv8bitSendAckRelease,
    Reset,
    ResetClk0
};

void enable();
void reset();
void init();
void sendByte(uint8_t v);
void recvByte(bool withAck = true);
uint8_t getByte();
State do_smL1();
}
