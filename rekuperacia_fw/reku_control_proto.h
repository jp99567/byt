#pragma once

#include <stdint.h>
#include "reku_enum.h"

#ifdef __cplusplus
namespace reku {
#pragma pack(push,1)
constexpr double crystalFoscHz = 4.9152e6;
#endif

const uint8_t mark_mask = 0xFC;
const uint8_t ctrl_bypass = 1<<0;
const uint8_t ctrl_off = 1<<1;

enum CommState { NO_COMM, COMMUNICATING, COMM_LOST };

const uint8_t markCmd = 0x54;
const uint8_t markCnf = 0x8C;
const uint8_t markInd = 0x4C;

struct RekuRx {
    uint8_t ctrl;
    uint8_t pwm[2];
    uint8_t crc;
};

struct VentCh {
    uint16_t period;
    uint16_t temp;
};

struct RekuTx {
    uint8_t stat;
    struct VentCh ch[2];
    uint8_t crc;
};


#ifdef __cplusplus
#pragma pack(pop)
} //namespace reku
#endif
