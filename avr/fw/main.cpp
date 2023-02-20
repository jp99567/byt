#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

#include "ow_sm.h"
#include "SvcProtocol_generated.h"
#include "gpio.h"
#include "log_debug.h"

uint32_t gCounter;
uint8_t gEvents;
namespace Event {
enum {
	timer = _BV(0)
};}

ISR(OVRIT_vect){
	gEvents |= Event::timer;
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
	int16_t value;
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
ow::RomData* gOwTempSensors;
uint8_t* gIOData;
uint8_t* gMobEndIdx;
uint16_t gMobMarkedTx;

namespace ow {
int gOwBitsCount;
uint8_t gOwData[12];
}

static uint8_t svc_msg[8] __attribute__ ((section (".noinit")));
static uint8_t svc_msglen __attribute__ ((section (".noinit")));

uint8_t can_rx_svc()
{
	CANPAGE = CANHPMOB;
	if(CANSTMOB & _BV(RXOK)){
		uint8_t id = CANPAGE >> 4;
		if(id==13 || id<=14){
			svc_msglen = CANCDMOB & 0x0F;
			for(uint8_t i=0; i<svc_msglen; ++i)
				svc_msg[i] = CANMSG;
			CANSTMOB = 0;
			CANCDMOB = ( 1 << CONMOB1) | ( 1 << IDE );
			if(svc_msglen > 0){
				DBG("rx ok: len:%u id:%u", svc_msglen, id);
				return id;
			}
		}
	}
	return 0;
}

void can_tx_svc(void) {
	if(not (svc_msglen > 0 && svc_msglen <= 8))
		return;
	CANPAGE = ( 12 << MOBNB0 );
	while ( CANEN1 & ( 1 << ENMOB12 ) ); // Wait for MOb 0 to be free
	CANSTMOB = 0x00;    	// Clear mob status register
	for(uint8_t i=0; i<svc_msglen; ++i)
		CANMSG = svc_msg[i];

	CANCDMOB = ( 1 << CONMOB0 ) | ( 1 << IDE ) | ( svc_msglen << DLC0 );
	while ( ! ( CANSTMOB & ( 1 << TXOK ) ) );	// wait for TXOK flag set
	//CANCDMOB = 0x00;
	CANSTMOB = 0x00;
}

void init()
{
	CANTCON = 255;
	CANGIE |= _BV(ENRX)|_BV(ENTX)|_BV(ENOVRT);
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
	svc_msg[0] = Svc::Protocol::CmdStatus;
	svc_msglen = 1;
	svc_msg[svc_msglen++] = CANTEC;
	svc_msg[svc_msglen++] = CANREC;
	svc_msg[svc_msglen++] = MCUSR | gFwStage;
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
	switch (svc_msg[0]) {
	case Svc::Protocol::CmdStatus:
		prepareStatusMsg();
		break;
	case Svc::Protocol::CmdSetAllocCounts:
		gDigIN_count = svc_msg[1];
		gDigOUT_count = svc_msg[2];
		gOwT_count = svc_msg[3];
		gIOData_count = svc_msg[4];
		gMobFirstTx = svc_msg[5];
		gMobCount = svc_msg[6];
		svc_msglen = 1;
		break;
	case Svc::Protocol::CmdGetAllocCounts:
		svc_msglen = 1;
		svc_msg[svc_msglen++] = gDigIN_count;
		svc_msg[svc_msglen++] = gDigOUT_count;
		svc_msg[svc_msglen++] = gOwT_count;
		svc_msg[svc_msglen++] = gIOData_count;
		svc_msg[svc_msglen++] = gMobFirstTx;
		svc_msg[svc_msglen++] = gMobCount;
		break;
	case Svc::Protocol::CmdSetStage:
	{
		const auto newStage = svc_msg[1];
		switch(newStage){
		case 0:
		case cFwStageInit1:
		case cFwStageInit2:
		case cFwStageRunning:
		{
			svc_msg[2] = gFwStage;
			if(newStage != gFwStage && newStage == cFwStageRunning)
				enableCanPvExchange();
			gFwStage = newStage;
			svc_msglen = 3;
		}
			break;
		default:
			svc_msg[0] = Svc::Protocol::CmdInvalid;
			svc_msglen = 1;
		}
	}
		break;
	case Svc::Protocol::CmdSetOwObjParams:
	{
		auto& obj = gOwT_Obj[svc_msg[1]];
		obj.mobIdx = svc_msg[2];
		obj.iodataIdx = svc_msg[3];
		obj.value = ow::cInvalidValue;
		svc_msglen = 1;
	}
		break;
	case Svc::Protocol::CmdSetOwObjRomCode:
	{
		auto& obj = gOwTempSensors[svc_msg[1]];
		::memcpy(obj.serial, &svc_msg[2], 6);
		obj.family = 0x28;
		obj.crc = 0;
		for(uint8_t i=0; i<sizeof(ow::RomCode); ++i){
			obj.crc = ow::calc_crc(*reinterpret_cast<const uint8_t*>(&obj), obj.crc);
		}
		svc_msg[1] = obj.crc;
		svc_msglen = 2;
	}
		break;
	case Svc::Protocol::CmdSetDigINObjParams:
	{
		auto& obj = gDigIN_Obj[svc_msg[1]];
		obj.setParams(svc_msg);
		svc_msglen = 1;
	}
			break;
	case Svc::Protocol::CmdSetDigOUTObjParams:
	{
		auto& obj = gDigOUT_Obj[svc_msg[1]];
		obj.setParams(svc_msg);
		svc_msglen = 1;
	}
		break;
	case Svc::Protocol::CmdSetCanMob:
	{
		uint8_t mobIdx = svc_msg[1];
		uint8_t ioEndIdx = svc_msg[2];
		auto canid = *reinterpret_cast<const uint16_t*>(&svc_msg[3]);
		initMob(mobIdx, ioEndIdx, canid);
		svc_msglen = 1;
	}
		break;
	case Svc::Protocol::CmdTestGetDDR:
	{
		svc_msglen = 1;
		static_assert(1+test::_IoPortNr <= sizeof(svc_msg));
		for(int port=0; port < test::_IoPortNr; ++port){
			svc_msg[svc_msglen++] = test::getDDR(port);
		}
	}
		break;
	case Svc::Protocol::CmdTestSetDDR:
	{
		unsigned idx = 1, port = 0;
		while((port < test::_IoPortNr) && (idx < svc_msglen)){
			test::setDDR(port++, svc_msg[idx++]);
		}
		svc_msglen=1;
	}
		break;
	case Svc::Protocol::CmdTestGetPORT:
	{
		svc_msglen = 1;
		static_assert(1+test::_IoPortNr <= sizeof(svc_msg));
		for(int port=0; port < test::_IoPortNr; ++port){
			svc_msg[svc_msglen++] = test::getPORT(port);
		}
	}
		break;
	case Svc::Protocol::CmdTestSetPORT:
	{
		unsigned idx = 1, port = 0;
		while((port < test::_IoPortNr) && (idx < svc_msglen)){
			test::setPORT(port++, svc_msg[idx++]);
		}
		svc_msglen=1;
	}
		break;
	case Svc::Protocol::CmdTestGetPIN:
	{
		svc_msglen = 1;
		static_assert(1+test::_IoPortNr <= sizeof(svc_msg));
		for(int port=0; port < test::_IoPortNr; ++port){
			svc_msg[svc_msglen++] = test::getPIN(port);
		}
	}
		break;
	case Svc::Protocol::CmdTestSetPIN:
	{
		unsigned idx = 1, port = 0;
		while((port < test::_IoPortNr) && (idx < svc_msglen)){
			test::setPIN(port++, svc_msg[idx++]);
		}
		svc_msglen=1;
	}
		break;
	case Svc::Protocol::CmdGetGitVersion:
	{
		constexpr uint32_t minutes2 = ((FW_BUID_EPOCH - Svc::fw_build_epoch_base) / 60) << 1;
		static_assert(minutes2 < (1L << 24), "build time out of range");
		static_assert(FW_GIT_IS_DIRTY < 2 && FW_GIT_IS_DIRTY >= 0, "invalid dirty flag");
		*reinterpret_cast<uint32_t*>(&svc_msg[4]) = 0x00FFFFFF & (minutes2|FW_GIT_IS_DIRTY);
		*reinterpret_cast<uint32_t*>(&svc_msg[1]) = FW_GIT_HASH_SHORT;
		svc_msglen = 8;
	}
		break;
	case Svc::Protocol::CmdOwInit:
		ow::init();
		break;
	case Svc::Protocol::CmdOwGetReplyCode:
		svc_msglen = 1;
		svc_msg[svc_msglen++] = ow::response();
		svc_msg[svc_msglen++] = ow::gOwBitsCount;
		break;
	case Svc::Protocol::CmdOwGetData:
	{
		auto index = svc_msglen > 1 ? svc_msg[1] : 0;
		const auto size = (ow::gOwBitsCount+7)/8;
		DBG("CmdOwGetData %u in, index:%u", size, index);
		svc_msglen = 1;
		if(index > size){
			svc_msg[0] = Svc::Protocol::CmdInvalid;
		}
		else{
			while(index < size && svc_msglen < 8){
				svc_msg[svc_msglen++] = ow::gOwData[index++];
			}
		}
	}
		break;
	case Svc::Protocol::CmdOwReadBits:
	{
		auto bitcount = svc_msg[1];
		if(svc_msglen < 2 || bitcount > sizeof(ow::gOwData)*8){
			svc_msg[0] = Svc::Protocol::CmdInvalid;
		}
		else{
			ow::read_bits(bitcount);
		}
		svc_msglen = 1;
	}
		break;
	case Svc::Protocol::CmdOwWriteBits:
	{
		auto bytecount = svc_msglen-2;
		bool pwr_strong = svc_msg[1];
		svc_msglen = 1;
		if(bytecount > 0){
			::memcpy(&ow::gOwData, &svc_msg[2], bytecount);
			ow::write_bits(8*bytecount, pwr_strong);
		}
		else{
			svc_msg[0] = Svc::Protocol::CmdInvalid;
		}
	}
		break;
	case Svc::Protocol::CmdOwSearch:
	{
		if(svc_msglen == 2){
			bool direction = svc_msg[1];
			ow::search(direction);
		}
		else{
			svc_msg[0] = Svc::Protocol::CmdInvalid;
		}
		svc_msglen = 1;
	}
		break;
	default:
		svc_msg[1] = Svc::Protocol::CmdInvalid;
		svc_msglen = broadcast ? 0 : 1;
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

void onTimer()
{
}

void run()
{
	DBG("running %02X %02X mcusr:%02X cant:%u", CANGIT, gEvents, MCUCR, CANTIM);
	sei();
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
		if(gEvents & Event::timer){
			gEvents &= ~Event::timer;
			gCounter++;
			onTimer();
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

	uint16_t allocSize =
			gIOData_count
		  + gMobCount
		  + gDigIN_count * sizeof(DigIN_Obj)
		  + gDigOUT_count * sizeof(DigOUT_Obj)
		  + gOwT_count * sizeof(OwT_Obj)
		  + ((gOwT_count > 1) ? gOwT_count*sizeof(ow::RomData) : 0);

	uint8_t allocGData[allocSize];
	uint8_t* ptr = allocGData;

	gIOData = ptr;
	ptr += gIOData_count;
	::memset(gIOData, 0, gIOData_count);

	gMobEndIdx = ptr;
	ptr += gMobCount;

	gDigIN_Obj = (DigIN_Obj*)ptr;
	ptr += gDigIN_count * sizeof(DigIN_Obj);

	gDigOUT_Obj = (DigOUT_Obj*)ptr;
	ptr += gDigOUT_count * sizeof(DigOUT_Obj);

	gOwT_Obj = (OwT_Obj*)ptr;
	ptr += gDigOUT_count * sizeof(OwT_Obj);

	gOwTempSensors = (ow::RomData*)ptr;

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

struct ClockTimer255US {
};

struct ClockTimer65MS
{
	static uint8_t now()
	{
		return CANTIMH;
	}

	static constexpr uint8_t durMs(unsigned ms)
	{

		return (ms*1000)/(256L*256L);
	}

	static bool isTimeout(uint8_t t0, uint8_t dur)
	{
		return t0 + dur < now();
	}
};

namespace meas{
enum class State {
	idling,
	ow_initing_before_convert,
	ow_requesting_conver,
	ow_converting,
	ow_initing_before_read,
	ow_matching_rom,
	ow_reading_scratchpad
};

auto gState = State::idling;
uint8_t gT0;
uint8_t gOwCurSensorIdx;

void start_read()
{

}

void measure_ow_temp_sm()
{
	switch(gState){
	case State::idling:
		if(gOwT_count){
			ow::init();
			gState = State::ow_initing_before_convert;
		}
		break;
	case State::ow_initing_before_convert:
		if(ow::response() == ow::ResponseCode::eOwPresenceOk){
			ow::gOwData[0] = (uint8_t)ow::Cmd::SKIP_ROM;
			ow::gOwData[1] = (uint8_t)ow::Cmd::CONVERT;
			ow::write_bits(2*8, true);
			gState = State::ow_requesting_conver;
		}
		break;
	case State::ow_requesting_conver:
		if(ow::response() == ow::ResponseCode::eOwWriteBitsOk){
			gState = State::ow_converting;
			gT0 = ClockTimer65MS::now();
		}
		break;
	case State::ow_converting:
	{
		auto timeout = ClockTimer65MS::durMs(750);
		if(ClockTimer65MS::isTimeout(gT0, timeout)){
			gOwCurSensorIdx = 0;
			ow::init();
			gState = State::ow_initing_before_read;
		}
	}
		break;
	case State::ow_initing_before_read:
		if(ow::response() == ow::ResponseCode::eOwPresenceOk){
			uint8_t cnt = 0;
			if(gOwT_count > 1){
				ow::gOwData[cnt++] = (uint8_t)ow::Cmd::MATCH_ROM;
				const uint8_t* rcdata = (const uint8_t*)&gOwTempSensors[gOwCurSensorIdx];
				const uint8_t* rcdata_end = rcdata + sizeof(ow::RomData);
				while(rcdata < rcdata_end)
					ow::gOwData[cnt++] = *rcdata++;
			}
			else{
				ow::gOwData[cnt++] = (uint8_t)ow::Cmd::SKIP_ROM;
			}
			ow::gOwData[cnt++] = (uint8_t)ow::Cmd::READ_SCRATCHPAD;
			ow::write_bits(cnt*8);
			gState = State::ow_matching_rom;
		}
		break;
	case State::ow_matching_rom:
		if(ow::response() == ow::ResponseCode::eOwWriteBitsOk){
			ow::read_bits(sizeof(ow::ThermScratchpad)*8);
			gState = State::ow_reading_scratchpad;
		}
		break;
	case State::ow_reading_scratchpad:
		if(ow::response() == ow::ResponseCode::eOwReadBitsOk){
			auto spad = reinterpret_cast<const ow::ThermScratchpad*>(ow::gOwData);
			uint8_t crc = 0;
			for(uint8_t i=0; i<sizeof(*spad)-1; ++i){
				crc = ow::calc_crc(ow::gOwData[i], crc);
			}
			auto value = ow::cInvalidValue;
			if(crc == spad->crc){
				value = spad->temperature;
			}
			auto& obj = gOwT_Obj[gOwCurSensorIdx++];
			if(obj.value != value){
				obj.value = value;
			}
		}
	}
}
}
