#pragma once

namespace ow {

void ow_init(void);
void ow_search(bool direction);
void ow_read_bits(uint8_t count);
void ow_write_bits(uint8_t count, bool strong_power_req);
ResponseCode response();
}
