/*
 * data.cpp
 *
 *  Created on: Jan 10, 2015
 *      Author: j
 */

#include "data.h"
#include "Log.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <string.h>

namespace ow
{

static constexpr uint8_t one_wire_crc8_table[] = {
 0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
 157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
 35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
 190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
 70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
 219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
 101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
 248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
 140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
 17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
 175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
 50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
 202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
 87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
 233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
 116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

bool check_crc(const uint8_t* ptr, unsigned size, uint8_t crc)
{
	uint8_t prevcrc(0);
	for(unsigned i=0; i<size; ++i)
		prevcrc = one_wire_crc8_table[prevcrc ^ ptr[i]];

	return prevcrc == crc;
}

RomCode::operator std::string() const
 {
     std::stringstream s;
     using std::setw;
     s << std::hex << std::setfill('0') << setw(2) << (unsigned)family;
     for(auto i: serial)
    	 s << setw(2) << (unsigned)i;

     s << setw(2) << (unsigned)crc;
     return s.str();
 }

#ifdef NEAKY_DEBUG
static void hexdump(const void* data, unsigned len)
{
	const uint8_t* bytea = (const uint8_t*)data;
	std::cerr << "size:" << len << "| ";
	for(unsigned i=0; i<len; ++i){
		std::cerr << std::hex << std::setfill('0') << std::setw(2) << (unsigned)bytea[i] << ' ';
	}
	std::cerr << '\n' << std::dec;
}
#endif

bool RomCode::operator<(const RomCode& other) const
{
	return 0 < memcmp(this, &other, sizeof(other));
}

bool RomCode::operator==(const RomCode& other) const
{
	return 0 == memcmp(this, &other, sizeof(other));
}

RomCode& RomCode::operator=(const std::string& str)
{
//287fc80a06000074
	std::stringstream s(str);

	if(str.size() < sizeof(*this)*2)
		LogDIE("mismatch size {}", str);

	std::string u;
	uint8_t *c = (uint8_t*)this;
	while(c < (uint8_t*)this+sizeof(*this)){
		s >> std::setw(2) >> u;
		*c = std::stoi(u, nullptr, 16);
		c++;
	}

	if(!check_crc(*this))
		LogERR("mismatch crc {}", str);

	return *this;
}

Sample::Sample()
	:t(std::chrono::system_clock::now())
{}

Sample& Sample::operator=(const Sample&& that)
{
	if( this != &that){
		t = that.t;
		data = std::move(that.data);
	}
	return *this;
}

Sample::Sample(Sample&& that)
{
	*this = std::move(that);
}

void Sample::add(const RomCode& rc, const float v)
{
	data.push_back(std::tuple<RomCode,float>(rc, v));
}

}
