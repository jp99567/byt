#include "scd4x_sm.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>
#include <util/twi.h>

namespace scd4x {
namespace {
constexpr uint8_t address = 0x62 << 1;
constexpr uint8_t meas_start[] = { 0x21, 0xb1 };
constexpr uint8_t get_data_ready_status[] = { 0xe4, 0xb8 };
constexpr uint8_t read_measurement[] = {0xec, 0x05};
}

#pragma pack(push,1)

union {
	MeasData data;
	Item singleItem;
	uint8_t buf[sizeof(MeasData)];
}theData;

#pragma pack(pop)

uint16_t swap_bytes(uint16_t val)
{
	return (val >> 8) | (val << 8);
}

bool check_crc_word(const Item& msg){
	constexpr uint8_t len=2;
    uint8_t crc = 0xFF;
    for(uint8_t idx=0; idx<len; ++idx){
        crc ^= reinterpret_cast<const uint8_t*>(&msg.val)[idx];
        for(int8_t bit_idx=7; bit_idx>=0; --bit_idx){
            if(crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc == msg.crc;
}

int8_t byte_in;
int8_t byte_req_wr;
int8_t byte_req_rd;

void load_command_get_data_ready_status()
{
	theData.buf[0] = get_data_ready_status[0];
	theData.buf[1] = get_data_ready_status[1];
}

void load_command_meas_start()
{
	theData.buf[0] = meas_start[0];
	theData.buf[1] = meas_start[1];
}

void load_command_read_meas_data()
{
	theData.buf[0] = read_measurement[0];
	theData.buf[1] = read_measurement[1];
}

void enable()
{
	TWBR = 124;
	TWSR = 0x02;
	TWCR = _BV(TWEN);
}

static State state;

State getState()
{
	return state;
}

void disable()
{
	TWCR = _BV(TWINT);
}

uint8_t* get_buf()
{
	return theData.buf;
}

void start()
{
	TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWSTA);
	state = State::Pending_IO;
}

void init_get_status()
{
	byte_req_wr = sizeof(theData.singleItem.val);
	byte_req_rd = sizeof(theData.singleItem);
	load_command_get_data_ready_status();
	byte_in = 0;
	start();
}

void init_start_periodic_meas()
{
	byte_req_wr = sizeof(theData.singleItem.val);
	byte_req_rd = 0;
	load_command_meas_start();
	byte_in = 0;
	start();
}

void init_read_meas()
{
	byte_req_wr = sizeof(theData.singleItem.val);
	byte_req_rd = sizeof(theData.data);
	load_command_read_meas_data();
	byte_in = 0;
	start();
}

void init_i2c_generic_wr_rd(uint8_t len_wr, uint8_t len_rd)
{
	byte_req_wr = len_wr;
	byte_req_rd = len_rd;
	byte_in = 0;
	start();
}

bool check_crc_single_item()
{
	return check_crc_word(theData.singleItem);
}

bool check_crc_meas_data()
{
	return check_crc_word(theData.data.temp)
		&& check_crc_word(theData.data.rh)
		&& check_crc_word(theData.data.co2);
}

bool data_ready()
{
	return swap_bytes(theData.singleItem.val) & ((1 << 11)-1);
}

uint16_t get_temp()
{
	return swap_bytes(theData.data.temp.val);
}

uint16_t get_rh()
{
	return swap_bytes(theData.data.rh.val);
}

uint16_t get_co2()
{
	return swap_bytes(theData.data.co2.val);
}

}

ISR(TWI_vect){
	switch(TW_STATUS){
	case TW_START:
		TWDR = scd4x::address|TW_WRITE;
		TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT);
		break;
	case TW_REP_START:
		TWDR = scd4x::address|TW_READ;
		TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT);
		break;
	case TW_MT_SLA_ACK:
	case TW_MT_DATA_ACK:
		if(scd4x::byte_in < scd4x::byte_req_wr){
			TWDR = scd4x::theData.buf[scd4x::byte_in++];
			TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT);
		}
		else{
			scd4x::byte_in = 0;
			if(scd4x::byte_req_rd){
				TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWSTA);
			}
			else{
				TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWSTO);
				scd4x::state = scd4x::State::Idle;
			}
		}
		break;
	case TW_MR_SLA_ACK:
		TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWEA);
		break;
	case TW_MR_DATA_ACK:
	{
		if(scd4x::byte_in < scd4x::byte_req_rd){
			scd4x::theData.buf[scd4x::byte_in++] = TWDR;
			if(scd4x::byte_in+1 != scd4x::byte_req_rd)
				TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWEA);
			else
				TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT);
		}
		else{
			scd4x::byte_in = 0;
			TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWSTO);
			scd4x::state = scd4x::State::Error;
		}
	}
		break;
	case TW_MR_DATA_NACK:
	{
		scd4x::theData.buf[scd4x::byte_in++] = TWDR;
		if(scd4x::byte_in == scd4x::byte_req_rd){
			scd4x::state = scd4x::State::Idle;
		}
		else{
			scd4x::state = scd4x::State::Error;
		}
		scd4x::byte_in = 0;
		TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWSTO);
	}
		break;
	default:
		TWCR = _BV(TWEN)|_BV(TWIE)|_BV(TWINT)|_BV(TWSTO);
		scd4x::state = scd4x::State::Error;
		break;
	}
}
