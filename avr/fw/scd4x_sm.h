#pragma once
#include <stdint.h>

namespace scd4x {

enum class State : int8_t
{
	Idle,
	Pending_IO,
	Error
};

#pragma pack(push,1)
struct Item
{
	uint16_t val;
	uint8_t crc;
};

struct MeasData
{
	Item co2, temp, rh;
};
#pragma pack(pop)

void enable();
void disable();
State getState();
void init_start_periodic_meas();
void init_stop_periodic_meas();
void init_read_meas();
void init_get_status();
bool check_crc_single_item();
bool check_crc_meas_data();
bool data_ready();
uint16_t get_temp();
uint16_t get_rh();
uint16_t get_co2();
uint8_t* get_buf();
void init_i2c_generic_wr_rd(uint8_t len_wr, uint8_t len_rd);
void power_off();
void power_on();

}

