#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

#include "ow.h"
#include "SvcProtocol_generated.h"

ISR(TIMER0_OVF_vect){
}

constexpr uint8_t cFwStageInit1 = _BV(7);
constexpr uint8_t cFwStageInit2 = _BV(6);
constexpr uint8_t cFwStageRunning = _BV(6) | _BV(7);
static uint8_t gFwStage = cFwStageInit1;

struct Base_Obj
{
	uint8_t mobIdx;
	uint8_t iodataIdx;
};

struct Dig_Obj : Base_Obj
{
	uint8_t mask;
	uint8_t pin;

	void setParams(const uint8_t* msg)
	{
		mobIdx = msg[2];
		iodataIdx = msg[3];
		mask = msg[4];
		pin = msg[5];
	}
};

using DigIN_Obj = Dig_Obj;
using DigOUT_Obj = Dig_Obj;

struct OwT_Obj : Base_Obj
{
	ow::RomCode rc;
};

static uint8_t gDigIN_count;
static uint8_t gDigOUT_count;
static uint8_t gOwT_count;
static uint8_t gIOData_count;
static uint8_t gMobFirstTx;
static uint8_t gMobCount;

DigIN_Obj* gDigIN_Obj;
DigOUT_Obj* gDigOUT_Obj;
OwT_Obj* gOwT_Obj;
uint8_t* gIOData;
uint16_t gMobMarkedUpdated;

namespace ow {
unsigned timer(void)
{
	return 0;
}

void send_status(enum ResponseCode code)
{

}

void send_status_with_data(enum ResponseCode code)
{

}

void send_status_with_param(enum ResponseCode code)
{

}

int gOwBitsCount;
uint8_t gOwData[12];

}

constexpr uint8_t u8min(uint8_t a, uint8_t b){
	return a < b ? a : b;
}

static uint8_t msg[8] __attribute__ ((section (".noinit")));
static uint8_t msglen __attribute__ ((section (".noinit")));
bool svc_rx_broadcast = false;
bool can_rx_svc()
{
	for(uint8_t id=13; id<=14; ++id){
		CANPAGE = ( id << MOBNB0 );
		if(CANSTMOB & _BV(RXOK)){
			msglen = u8min(CANCDMOB & 0x0F, 8);

			for(uint8_t i=0; i<msglen; ++i)
				msg[i] = CANMSG;
			CANSTMOB = 0;
			CANCDMOB = ( 1 << CONMOB1) | ( 1 << IDE );
			if(msglen > 0){
				svc_rx_broadcast = id == 14;
				return true;
			}
		}
	}
	return false;
}

void can_tx_svc(void) {
	if(not msglen or msglen > 8)
		return;
	CANPAGE = ( 12 << MOBNB0 );
	while ( CANEN1 & ( 1 << ENMOB12 ) ); // Wait for MOb 0 to be free
	CANSTMOB = 0x00;    	// Clear mob status register
	for(uint8_t i=0; i<msglen; ++i)
		CANMSG = msg[i];

	CANCDMOB = ( 1 << CONMOB0 ) | ( 1 << IDE ) | ( msglen << DLC0 );
	while ( ! ( CANSTMOB & ( 1 << TXOK ) ) );	// wait for TXOK flag set
	CANCDMOB = 0x00;
	CANSTMOB = 0x00;
}

void init()
{
	CANTCON = 99;
}

void svc() {
	switch (msg[0]) {
	case Svc::Protocol::CmdStatus:
		msglen = 1;
		msg[msglen++] = CANTEC;
		msg[msglen++] = CANREC;
		msg[msglen++] = MCUSR | gFwStage;
		break;
	case Svc::Protocol::CmdSetAllocCounts:
		gDigIN_count = msg[1];
		gDigOUT_count = msg[2];
		gOwT_count = msg[3];
		gIOData_count = msg[4];
		gMobFirstTx = msg[5];
		gMobCount = msg[6];
		msglen = 1;
		break;
	case Svc::Protocol::CmdGetAllocCounts:
		msglen = 1;
		msg[msglen++] = gDigIN_count;
		msg[msglen++] = gDigOUT_count;
		msg[msglen++] = gOwT_count;
		msg[msglen++] = gIOData_count;
		msg[msglen++] = gMobFirstTx;
		msg[msglen++] = gMobCount;
		break;
	case Svc::Protocol::CmdSetStage:
		gFwStage = msg[1];
		msglen = 1;
		break;
	case Svc::Protocol::CmdSetOwObjParams:
	{
		auto& obj = gOwT_Obj[msg[1]];
		obj.mobIdx = msg[2];
		obj.iodataIdx = msg[3];
		msglen = 1;
	}
		break;
	case Svc::Protocol::CmdSetOwObjRomCode:
		{
			auto& obj = gOwT_Obj[msg[1]];
			obj.rc = *reinterpret_cast<const ow::RomCode*>(&msg[2]);
			msglen = 1;
		}
			break;
	case Svc::Protocol::CmdSetDigINObjParams:
		{
			auto& obj = gDigIN_Obj[msg[1]];
			obj.setParams(msg);
			msglen = 1;
		}
			break;
	case Svc::Protocol::CmdSetDigOUTObjParams:
		{
			auto& obj = gDigOUT_Obj[msg[1]];
			obj.setParams(msg);
			//setDir(obj.pin);ToDo
			msglen = 1;
		}
			break;
	default:
		msg[1] = Svc::Protocol::CmdInvalid;
		msglen = svc_rx_broadcast ? 0 : 1;
		break;
	}

	can_tx_svc();
}

void setDir(uint8_t pin)
{

}

bool getDigInVal(uint8_t pin)
{
	uint8_t portid = pin >> 3;
	uint8_t mask = 1 << (pin & 0b111);

		switch(portid){
		case 0:	return PINA & mask;
		case 1: return PINB & mask;
		case 2: return PINC & mask;
		case 3: return PIND & mask;
		case 4: return PINE & mask;
		case 5:	return PINF & mask;
		case 6:	return PING & mask;
		default:
			return false;
		}
}

void markMob(uint8_t idx)
{
	gMobMarkedUpdated |= _BV(idx);
}

bool isMobMarked(uint8_t idx)
{
	return gMobMarkedUpdated & _BV(idx);
}

void run()
{
	while(1){
		for(int i=0; i<gDigIN_count; ++i){
			bool v = getDigInVal(gDigIN_Obj[i].pin);
			auto& iobyte = gIOData[gDigIN_Obj[i].iodataIdx];
			auto mask = gDigIN_Obj[i].mask;
			if( (bool)(iobyte & mask) == v){
				if(v)
					iobyte |= mask;
				else
					iobyte &= ~mask;
				markMob(gDigIN_Obj[i].mobIdx);
			}
		}

		for(uint8_t mobidx = gMobFirstTx; mobidx < gMobCount; ++mobidx){
			if(isMobMarked(mobidx)){
				CANPAGE = ( mobidx << MOBNB0 );
				if(not ( CANEN1 & ( 1 << ENMOB12 ) )){

				}
			}
		}
	}
}

int main() {

	init();

	while(gFwStage == cFwStageInit1){
		if(can_rx_svc()){
			svc();
		}
	}

	DigIN_Obj allocDigIN_Obj[gDigIN_count];
	gDigIN_Obj = allocDigIN_Obj;

	DigOUT_Obj allocDigOUT_Obj[gDigOUT_count];
	gDigOUT_Obj = allocDigOUT_Obj;

	OwT_Obj allocOwT_Obj[gOwT_count];
	gOwT_Obj = allocOwT_Obj;

	uint8_t allocIOData[gIOData_count];
	gIOData = allocIOData;
	::memset(gIOData, 0, gIOData_count);

	while(gFwStage == cFwStageInit2){
		if(can_rx_svc()){
			svc();
		}
	}

	run();

	return 0;
}

