#pragma once

namespace sht11 {

void init_hw();
void clk0();
void clk1();
void data_pull();
void data_release();
bool data();
}
