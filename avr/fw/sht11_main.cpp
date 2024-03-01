#include "sht11_sm.h"

namespace {

uint8_t reverse_bits(uint8_t v)
{
    auto r = v;
    int s = 7;
    for(v >>= 1; v; v >>= 1) {
        r <<= 1;
        r |= v & 1;
        s--;
    }
    r <<= s;
    return r;
}

uint8_t calc_crc_8bit(uint8_t crc_prev, uint8_t byte)
{
    for(int bit = 7; bit >= 0; --bit) {
        auto new_bit = byte >> 7;
        byte <<= 1;
        auto crc_bit7 = crc_prev >> 7;
        auto differ = new_bit ^ crc_bit7;
        if(differ) {
            const uint8_t mask2 = (differ << 3) | (differ << 4);
            const uint8_t mask = (1 << 3) | (1 << 4);
            auto tmp_crc = crc_prev ^ mask2;
            crc_prev &= ~mask;
            crc_prev |= tmp_crc & mask;
        }
        crc_prev <<= 1;
        crc_prev |= differ;
    }
    return crc_prev;
}
}

namespace sht11 {

constexpr uint8_t cmdMeasTemp = 0b00000011;
constexpr uint8_t cmdMeasRH = 0b00000101;
constexpr auto time_quantum = 1; //ToDo
unsigned t_count = 0;
unsigned t0 = 0;

bool timeout(unsigned timeout)
{
    unsigned dt = t_count - t0;
    return timeout <= dt;
}

void reset_time()
{
    t0 = t_count;
}

constexpr auto timeout5s = 5u; //ToDo
constexpr auto timeout500ms = 500u; //ToDo
constexpr auto timeout50ms = 50u; //ToDo

enum class StateL2 {
    Disabled,
    Relax,
    MeasT_Init,
    MeasT_SendCmd,
    MeasT_ReadH,
    MeasT_ReadL,
    MeasT_ReadCRC,
    MeasRH_Init,
    MeasRH_SendCmd,
    MeasRH_ReadH,
    MeasRH_ReadL,
    MeasRH_ReadCRC,
    ClearError
};

static auto state = StateL2::Relax;
static uint16_t sTemp;
static uint16_t sRH;

bool eval_state_helper()
{
    auto stateL1 = do_smL1();
    if(stateL1 == State::Idle) {
        return true;
    } else if(stateL1 == State::Error) {
        reset_time();
        state = StateL2::Relax;
    }
    return false;
}

void do_sm()
{
    switch(state) {
    case StateL2::Disabled:
        break;
    case StateL2::Relax:
        if(timeout(timeout5s)) {
            auto stateL1 = do_smL1();
            if(stateL1 != State::Idle) {
                reset();
                state = StateL2::ClearError;
            } else {
                init();
                state = StateL2::MeasT_Init;
            }
        }
        break;
    case StateL2::MeasT_Init:
        if(eval_state_helper()) {
            sendByte(cmdMeasTemp);
            reset_time();
            state = StateL2::MeasT_SendCmd;
        }
        break;
    case StateL2::MeasT_SendCmd: {
        auto stateL1 = do_smL1();
        if(stateL1 == State::Idle) {
            recvByte();
            state = StateL2::MeasT_ReadH;
        } else if(stateL1 == State::Error) {
            reset_time();
            state = StateL2::Relax;
        } else if(timeout(timeout500ms)) {
            reset();
            state = StateL2::ClearError;
        }
    } break;
    case StateL2::MeasT_ReadH:
        if(eval_state_helper()) {
            sTemp = (uint16_t)getByte() << 8;
            recvByte();
            state = StateL2::MeasT_ReadL;
        }
        break;
    case StateL2::MeasT_ReadL:
        if(eval_state_helper()) {
            sTemp |= (uint16_t)getByte();
            recvByte(false);
            state = StateL2::MeasT_ReadCRC;
        }
        break;
    case StateL2::MeasT_ReadCRC:
        if(eval_state_helper()) {
            uint8_t crc = calc_crc_8bit(0, cmdMeasTemp);
            crc = calc_crc_8bit(crc, sTemp >> 8);
            crc = calc_crc_8bit(crc, sTemp & 0xFF);
            crc = reverse_bits(crc);
            if(crc == getByte()) {
                reset_time();
                init();
                state = StateL2::MeasRH_Init;
            } else {
                reset();
                state = StateL2::ClearError;
            }
        }
        break;
        ///////////////////////
    case StateL2::MeasRH_Init:
        if(eval_state_helper()) {
            sendByte(cmdMeasRH);
            reset_time();
            state = StateL2::MeasRH_SendCmd;
        }
        break;
    case StateL2::MeasRH_SendCmd: {
        auto stateL1 = do_smL1();
        if(stateL1 == State::Idle) {
            recvByte();
            state = StateL2::MeasRH_ReadH;
        } else if(stateL1 == State::Error) {
            reset_time();
            state = StateL2::Relax;
        } else if(timeout(timeout500ms)) {
            reset();
            state = StateL2::ClearError;
        }
    } break;
    case StateL2::MeasRH_ReadH:
        if(eval_state_helper()) {
            sRH = (uint16_t)getByte() << 8;
            recvByte();
            state = StateL2::MeasRH_ReadL;
        }
        break;
    case StateL2::MeasRH_ReadL:
        if(eval_state_helper()) {
            sRH |= (uint16_t)getByte();
            recvByte(false);
            state = StateL2::MeasRH_ReadCRC;
        }
        break;
    case StateL2::MeasRH_ReadCRC:
        if(eval_state_helper()) {
            uint8_t crc = calc_crc_8bit(0, cmdMeasRH);
            crc = calc_crc_8bit(crc, sRH >> 8);
            crc = calc_crc_8bit(crc, sRH & 0xFF);
            crc = reverse_bits(crc);
            if(crc == getByte()) {
                reset_time();
                state = StateL2::Relax;
            } else {
                reset();
                state = StateL2::ClearError;
            }
        }
        break;
        /////////////////////
    case StateL2::ClearError: {
        auto stateL1 = do_smL1();
        if(stateL1 == State::Idle || stateL1 == State::Error) {
            reset_time();
            state = StateL2::Relax;
        }
    } break;
    default:
        break;
    }
    ++t_count;
}
}
