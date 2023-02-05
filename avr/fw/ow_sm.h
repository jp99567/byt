#pragma once

#include "ow.h"

namespace ow {

void init(void);
void search(bool direction);
void read_bits(uint8_t count);
void write_bits(uint8_t count, bool strong_power_req);
ResponseCode response();
}

//
