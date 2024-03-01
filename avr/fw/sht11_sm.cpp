#include "sht11_sm.h"
#include "sht11_swi2c.h"

#ifdef linux
#include <spdlog/spdlog.h>
#endif

namespace sht11 {

#ifdef linux

void enable()
{
    init_hw();
}

#else
void enable()
{
    \\ToDo
}
#endif

static State state = State::Idle;
uint8_t byte;
uint8_t bit_cnt;
bool sWithAck;

State do_smL1()
{
    switch(state) {
    case State::Idle:
        break;
    case State::InitTransfer:
        clk1();
        state = State::InitTransferDataPull;
        break;
    case State::InitTransferDataPull:
        data_pull();
        state = State::InitTransferLowPulseClk0;
        break;
    case State::InitTransferLowPulseClk0:
        clk0();
        state = State::InitTransferLowPulseClk1;
        break;
    case State::InitTransferLowPulseClk1:
        clk1();
        state = State::InitTransferDataRelease;
        break;
    case State::InitTransferDataRelease:
        data_release();
        state = State::InitTransferClk0;
        break;
    case State::InitTransferClk0:
        clk0();
        state = State::Finising;
        break;
    case State::Finising:
        state = State::Idle;
        break;
    case State::Send8bit:
        if(byte & 0x80)
            data_release();
        else
            data_pull();
        byte <<= 1;
        state = State::Send8bitHiPulseClk1;
        break;
    case State::Send8bitHiPulseClk1:
        clk1();
        state = State::Send8bitHiPulseClk0;
        break;
    case State::Send8bitHiPulseClk0:
        clk0();
        if(++bit_cnt < 8)
            state = State::Send8bit;
        else
            state = State::Send8bitReleaseBAck;
        break;
    case State::Send8bitReleaseBAck:
        data_release();
        state = State::Send8bitHiPluseAckClk1;
        break;
    case State::Send8bitHiPluseAckClk1:
        clk1();
        state = State::Send8bitHiPluseAckClk0;
        break;
    case State::Send8bitHiPluseAckClk0: {
        bool ack = !data();
        clk0();
        SPDLOG_INFO("sendbyte ack:{}", ack);
        if(ack)
            state = State::Send8bitWaitMeas;
        else
            state = state = State::Error;
    } break;
    case State::Send8bitWaitMeas:
        if(!data())
            state = State::Finising;
        break;
    case State::Recv8bit:
        state = State::Recv8HiPulseClk1;
        break;
    case State::Recv8HiPulseClk1:
        clk1();
        state = State::Recv8HiPulseClk0;
        break;
    case State::Recv8HiPulseClk0: {
        uint8_t bit = data() ? 1 : 0;
        byte |= bit << (7 - bit_cnt);
        clk0();
        if(++bit_cnt < 8)
            state = State::Recv8bit;
        else if(sWithAck)
            state = State::Recv8bitSendAck;
        else
            state = State::Finising;
    } break;
    case State::Recv8bitSendAck:
        data_pull();
        state = State::Recv8bitSendAckClk1;
        break;
    case State::Recv8bitSendAckClk1:
        clk1();
        state = State::Recv8bitSendAckClk0;
        break;
    case State::Recv8bitSendAckClk0:
        clk0();
        state = State::Recv8bitSendAckRelease;
        break;
    case State::Recv8bitSendAckRelease:
        data_release();
        state = State::Finising;
        break;
    case State::Reset:
        clk1();
        state = State::ResetClk0;
        break;
    case State::ResetClk0:
        clk0();
        if(++bit_cnt < 12)
            state = State::Reset;
        else
            state = State::Finising;
        break;
    case State::Error:
    default:
        break;
    }
    return state;
}

void init()
{
    state = State::InitTransfer;
}

void sendByte(uint8_t v)
{
    byte = v;
    bit_cnt = 0;
    state = State::Send8bit;
}

uint8_t getByte()
{
    return byte;
}

void recvByte(bool withAck)
{
    byte = 0;
    bit_cnt = 0;
    sWithAck = withAck;
    state = State::Recv8bit;
}

void reset()
{
    bit_cnt = 0;
    data_release();
    clk0();
    SPDLOG_INFO("sw reset sht11");
    state = State::Reset;
}
}
