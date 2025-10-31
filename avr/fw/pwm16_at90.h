#pragma once
#include <stdint.h>

namespace Pwm16At90 {

enum class Channel:uint8_t{ PE3_OC3A, PE4_OC3B, PE5_OC3C };
void init(uint8_t prescaler, uint16_t top);
void enableOut(Channel ch);
void setPwm(Channel ch, uint16_t val);

}
