#pragma once

#include <avr/io.h>

bool getDigInVal(uint8_t pin);
void setDirOut(uint8_t pin);
void setDigOut(uint8_t pin, bool v);

namespace test {
enum PortList : uint8_t {
	PA,PB,PC,PD,PE,PF,PG,_IoPortNr
};
void setDDR(uint8_t portid, uint8_t val);
void setPORT(uint8_t portid, uint8_t val);
void setPIN(uint8_t portid, uint8_t val);
uint8_t getDDR(uint8_t portid);
uint8_t getPORT(uint8_t portid);
uint8_t getPIN(uint8_t portid);
}
