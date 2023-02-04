#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

#include "ow.h"
#include "SvcProtocol_generated.h"
#include "gpio.h"
#include "log_debug.h"

ISR(TIMER0_OVF_vect){
}

constexpr uint8_t cFwStageInit1 = _BV(6);
constexpr uint8_t cFwStageInit2 = _BV(7);
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

struct DigOUT_Obj : Dig_Obj
{
	void setParams(const uint8_t* msg)
	{
		Dig_Obj::setParams(msg);
		setDirOut(pin);
	}
};

using DigIN_Obj = Dig_Obj;

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
uint8_t* gMobEndIdx;
uint16_t gMobMarkedTx;

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

uint8_t can_rx_svc()
{
	CANPAGE = CANHPMOB;
	if(CANSTMOB & _BV(RXOK)){
		uint8_t id = CANPAGE >> 4;
		if(id==13 || id<=14){
			msglen = u8min(CANCDMOB & 0x0F, 8);
			for(uint8_t i=0; i<msglen; ++i)
				msg[i] = CANMSG;
			CANSTMOB = 0;
			CANCDMOB = ( 1 << CONMOB1) | ( 1 << IDE );
			if(msglen > 0){
				return id;
			}
		}
	}
	return 0;
}

void can_tx_svc(void) {
	if(not (msglen > 0 && msglen <= 8))
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
	CANGIE |= _BV(ENRX)|_BV(ENTX);
	CANIE |= _BV(13) | _BV(14);
	init_log_debug();
}

void initMob(uint8_t mobIdx, uint8_t endIoDataIdx, uint16_t canid11bit)
{
	CANPAGE = ( mobIdx << MOBNB0 );
	constexpr uint32_t mask = 0b1111'1111'1110'0000'0000'0000'0000'0101;
	CANIDM = mask;
	CANIDT = (uint32_t)canid11bit << (32-11);
	gMobEndIdx[mobIdx] = endIoDataIdx;
}

void prepareStatusMsg()
{
	msg[0] = Svc::Protocol::CmdStatus;
	msglen = 1;
	msg[msglen++] = CANTEC;
	msg[msglen++] = CANREC;
	msg[msglen++] = MCUSR | gFwStage;
}

void enableCanPvExchange()
{
	auto mobidx = 0;
	while(mobidx < gMobFirstTx){
		const uint8_t beginIoIdx = mobidx > 0
						? gMobEndIdx[mobidx-1] : 0;
		const uint8_t endIoIdx = gMobEndIdx[mobidx];
		const uint8_t size =  endIoIdx - beginIoIdx;
		CANPAGE = ( mobidx << MOBNB0 );
		CANCDMOB = _BV(CONMOB1)|size; //enable RX
		CANIE |= _BV(mobidx++);
	}
	while(mobidx < gMobCount){
		const uint8_t beginIoIdx = mobidx > 0
				? gMobEndIdx[mobidx-1] : 0;
		const uint8_t endIoIdx = gMobEndIdx[mobidx];
		const uint8_t size =  endIoIdx - beginIoIdx;
		CANPAGE = ( mobidx << MOBNB0 );
		CANCDMOB = size;
		CANIE |= _BV(mobidx++);
	}
}

void svc(bool broadcast) {
	switch (msg[0]) {
	case Svc::Protocol::CmdStatus:
		prepareStatusMsg();
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
	{
		const auto newStage = msg[1];
		switch(newStage){
		case 0:
		case cFwStageInit1:
		case cFwStageInit2:
		case cFwStageRunning:
		{
			msg[2] = gFwStage;
			if(newStage != gFwStage && newStage == cFwStageRunning)
				enableCanPvExchange();
			gFwStage = newStage;
			msglen = 3;
		}
			break;
		default:
			msg[0] = Svc::Protocol::CmdInvalid;
			msglen = 1;
		}
	}
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
		msglen = 1;
	}
		break;
	case Svc::Protocol::CmdSetCanMob:
	{
		uint8_t mobIdx = msg[1];
		uint8_t ioEndIdx = msg[2];
		auto canid = *reinterpret_cast<const uint16_t*>(&msg[3]);
		initMob(mobIdx, ioEndIdx, canid);
		msglen = 1;
	}
		break;
	case Svc::Protocol::CmdTestGetDDR:
	{
		msglen = 1;
		static_assert(1+test::_IoPortNr <= sizeof(msg));
		for(int port=0; port < test::_IoPortNr; ++port){
			msg[msglen++] = test::getDDR(port);
		}
	}
		break;
	case Svc::Protocol::CmdTestSetDDR:
	{
		unsigned idx = 1, port = 0;
		while((port < test::_IoPortNr) && (idx < msglen)){
			test::setDDR(port++, msg[idx++]);
		}
		msglen=1;
	}
		break;
	case Svc::Protocol::CmdTestGetPORT:
	{
		msglen = 1;
		static_assert(1+test::_IoPortNr <= sizeof(msg));
		for(int port=0; port < test::_IoPortNr; ++port){
			msg[msglen++] = test::getPORT(port);
		}
	}
		break;
	case Svc::Protocol::CmdTestSetPORT:
	{
		unsigned idx = 1, port = 0;
		while((port < test::_IoPortNr) && (idx < msglen)){
			test::setPORT(port++, msg[idx++]);
		}
		msglen=1;
	}
		break;
	case Svc::Protocol::CmdTestGetPIN:
	{
		msglen = 1;
		static_assert(1+test::_IoPortNr <= sizeof(msg));
		for(int port=0; port < test::_IoPortNr; ++port){
			msg[msglen++] = test::getPIN(port);
		}
	}
		break;
	case Svc::Protocol::CmdTestSetPIN:
	{
		unsigned idx = 1, port = 0;
		while((port < test::_IoPortNr) && (idx < msglen)){
			test::setPIN(port++, msg[idx++]);
		}
		msglen=1;
	}
		break;
	default:
		msg[1] = Svc::Protocol::CmdInvalid;
		msglen = broadcast ? 0 : 1;
		break;
	}

	can_tx_svc();
}

void markMobTx(uint8_t idx)
{
	gMobMarkedTx |= _BV(idx);
}

bool isMobMarkedTx(uint8_t idx)
{
	return gMobMarkedTx & _BV(idx);
}

void clearMarkTxMob(uint8_t idx)
{
	gMobMarkedTx &= ~_BV(idx);
}

void iterateOutputObjs(uint8_t mobidx)
{
	for(int i=0; i<gDigOUT_count; ++i){
		auto& obj = gDigOUT_Obj[i];
		if(mobidx == obj.mobIdx){
			bool v = gIOData[obj.iodataIdx] & obj.mask;
			setDigOut(obj.pin, v);
		}
	}
}

void run()
{
	while(1){
		if(CANGIT & _BV(CANIT))
		{
			{
				auto id = can_rx_svc();
				if(id)
					svc(id==14);
			}

			uint8_t mobidx=0;
			while(mobidx < gMobFirstTx){ //iterate rx mobs
				if(CANSIT & _BV(mobidx)){
					CANPAGE = ( mobidx << MOBNB0 );
					if(CANSTMOB & _BV(RXOK)){
						const uint8_t beginIdx = mobidx > 0
								? gMobEndIdx[mobidx-1] : 0;
						const uint8_t endIdx = gMobEndIdx[mobidx];
						auto idx = beginIdx;
						auto dlc = CANCDMOB & 0x0F;
						while((idx < endIdx) && ((idx-beginIdx) < dlc))
							gIOData[idx++] = CANMSG;
						CANSTMOB = 0;
						CANCDMOB = ( 1 << CONMOB1 ) | ( (endIdx-beginIdx) << DLC0 );
						iterateOutputObjs(mobidx++);
						continue;
					}
					CANSTMOB = 0;
				}
				++mobidx;
			}
			while(mobidx < gMobCount){ //iterate and clear tx mobs
				if(CANSIT & _BV(mobidx)){
					CANPAGE = ( mobidx << MOBNB0 );
					if(CANSTMOB){
						CANCDMOB = 0x00;
						CANSTMOB = 0x00;
					}
				}
				++mobidx;
			}
		}

		for(int i=0; i<gDigIN_count; ++i){
			bool v = getDigInVal(gDigIN_Obj[i].pin);
			auto& iobyte = gIOData[gDigIN_Obj[i].iodataIdx];
			auto mask = gDigIN_Obj[i].mask;
			if( (bool)(iobyte & mask) != v){
				if(v)
					iobyte |= mask;
				else
					iobyte &= ~mask;
				markMobTx(gDigIN_Obj[i].mobIdx);
			}
		}

		for(uint8_t mobidx = gMobFirstTx; mobidx < gMobCount; ++mobidx){
			if(isMobMarkedTx(mobidx)){
				if(not ( CANEN & ( 1 << mobidx ) )){
					CANPAGE = ( mobidx << MOBNB0 );
					CANSTMOB = 0;
					const uint8_t beginIdx = mobidx > 0
							? gMobEndIdx[mobidx-1] : 0;
					const uint8_t endIdx = gMobEndIdx[mobidx];
					auto idx = beginIdx;
					while(idx < endIdx)
						CANMSG = gIOData[idx++];
					CANCDMOB = ( 1 << CONMOB0 ) | ( (endIdx-beginIdx) << DLC0 );
					clearMarkTxMob(mobidx);
				}
			}
		}
	}
}

int main() {

	init();

	prepareStatusMsg();
	can_tx_svc();

	while(gFwStage == cFwStageInit1){
		auto id = can_rx_svc();
		if(id){
			svc(id==14);
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

	uint8_t allocMobEndIdx[gMobCount];
	gMobEndIdx = allocMobEndIdx;

	while(gFwStage == cFwStageInit2){
		auto id = can_rx_svc();
		if(id){
			svc(id==14);
		}
	}

	prepareStatusMsg();
	can_tx_svc();

	run();

	return 0;
}

