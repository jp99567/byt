#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

#include "ow_sm.h"
#include "SvcProtocol_generated.h"
#include "gpio.h"
#include "log_debug.h"
#include "sensirion_common.h"
#include "clock.h"
#include "sht11_sm.h"

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

struct DigIN_Obj : Dig_Obj
{
	void setParams(const uint8_t* msg)
	{
		Dig_Obj::setParams(msg);
		setDigOut(pin, true); // pull up
	}
	uint8_t debounce = 3;
	uint8_t consecutive = 0;
};

struct OwT_Obj : Base_Obj
{
};

enum Features {
	eFeatureSCD41 = (1<<0),
	eFeatureSHT11 = (1<<1)
};

static uint8_t gDigIN_count;
static uint8_t gDigOUT_count;
static uint8_t gOwT_count;
static uint8_t gIOData_count;
static uint8_t gMobFirstTx;
static uint8_t gMobCount;
static uint8_t gFeatures;

struct SHT11_Obj;
struct SCD41_Obj;
DigIN_Obj* gDigIN_Obj;
DigOUT_Obj* gDigOUT_Obj;
OwT_Obj* gOwT_Obj;
ow::RomData* gOwTempSensors;
SCD41_Obj* gSCD41_Obj;
SHT11_Obj* gSHT11_Obj;
uint8_t* gIOData;
uint8_t* gMobEndIdx;
uint16_t gMobMarkedTx;

struct SHT11_Obj : Base_Obj
{
	uint16_t& valueT() {return reinterpret_cast<uint16_t*>(&gIOData[iodataIdx])[0];}
	uint16_t& valueRH() {return reinterpret_cast<uint16_t*>(&gIOData[iodataIdx])[1];}
};

struct SCD41_Obj : SHT11_Obj
{
	uint16_t& valueCO2() {return reinterpret_cast<uint16_t*>(&gIOData[iodataIdx])[2];}
};

namespace ow {
int gOwBitsCount;
uint8_t gOwData[12];
}

namespace meas{
void measure_ow_temp_sm();
}

static uint8_t svc_msg[8] __attribute__ ((section (".noinit")));
static uint8_t svc_msglen __attribute__ ((section (".noinit")));

uint8_t can_rx_svc()
{
	CANPAGE = CANHPMOB;
	if(CANSTMOB & _BV(RXOK)){
		uint8_t id = CANPAGE >> 4;
		if(id==13 || id==14){
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
		gFeatures = svc_msg[7];
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
		svc_msg[svc_msglen++] = gFeatures;
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
		auto& curValue = *reinterpret_cast<int16_t*>(&gIOData[obj.iodataIdx]);
		curValue = ow::cInvalidValue;
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
			obj.crc = ow::calc_crc(reinterpret_cast<const uint8_t*>(&obj)[i], obj.crc);
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
	case Svc::Protocol::CmdSetSCD41Params:
	{
		auto& obj = gSCD41_Obj[svc_msg[1]];
		obj.mobIdx = svc_msg[2];
		obj.iodataIdx = svc_msg[3];
		obj.valueT() = obj.valueRH() = obj.valueCO2() = Sensorion::cInvalidValue;
		svc_msglen = 1;
	}
		break;
	case Svc::Protocol::CmdSetSHT11Params:
	{
		auto& obj = gSHT11_Obj[svc_msg[1]];
		obj.mobIdx = svc_msg[2];
		obj.iodataIdx = svc_msg[3];
		obj.valueT() = obj.valueRH() = Sensorion::cInvalidValue;
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
		*reinterpret_cast<uint32_t*>(&svc_msg[4]) = ((minutes2|FW_GIT_IS_DIRTY) << 8);
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

void onTimer16s7()
{
}

void onTimer4ms096()
{
	meas::measure_ow_temp_sm();

	for(int i=0; i<gDigIN_count; ++i){
		bool v = getDigInVal(gDigIN_Obj[i].pin);
		auto& iobyte = gIOData[gDigIN_Obj[i].iodataIdx];
		const auto mask = gDigIN_Obj[i].mask;
		auto& consecutive = gDigIN_Obj[i].consecutive;
		if( (bool)(iobyte & mask) != v){
			const auto debounce = gDigIN_Obj[i].debounce;
			if(++consecutive > debounce){
				if(v)
					iobyte |= mask;
				else
					iobyte &= ~mask;
				markMobTx(gDigIN_Obj[i].mobIdx);
				consecutive = 0;
			}
		}
		else{
			consecutive = 0;
		}
	}
}

constexpr bool isSvcRx(uint8_t mobid)
{
	return mobid==13 || mobid==14;
}

namespace sht11{
void do_sm();
void enableL2();
}

void run()
{
	DBG("running %02X %02X mcusr:%02X cant:%u", CANGIT, gEvents, MCUCR, CANTIM);

	if(gFeatures & eFeatureSHT11){
		sht11::enableL2();
	}

	sei();

	uint8_t prevCANTIML_5b = CANTIML & 0xF8;

	while(1){
		if(CANGIT & _BV(CANIT))
		{
			uint8_t mobidx = CANHPMOB >> 4;
			CANPAGE = CANHPMOB;
			if( isSvcRx(mobidx))
			{
				auto id = can_rx_svc();
				if(id)
					svc(id==14);
			}

			if(mobidx < gMobFirstTx){ //iterate rx mobs
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
				}
				CANSTMOB = 0;
			}
			if(mobidx < gMobCount){ //iterate and clear tx mobs
				if(CANSIT & _BV(mobidx)){
					if(CANSTMOB){
						CANSTMOB = 0x00;
					}
				}
				++mobidx;
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
			onTimer16s7();
		}

		uint8_t curCANTIML_5b = CANTIML & 0xF8;
		if(curCANTIML_5b != prevCANTIML_5b){
			prevCANTIML_5b = curCANTIML_5b;
			onTimer4ms096();
			sht11::do_sm();
		}
	}
}

uint16_t sizeOfOwT()
{
	return ((gOwT_count > 1) ? gOwT_count*sizeof(ow::RomData) : 0);
}

uint16_t sizeOfSCD41()
{
	return (gFeatures & eFeatureSCD41) ? sizeof(*gSCD41_Obj) : 0;
}

uint16_t sizeOfSHT11()
{
	return (gFeatures & eFeatureSHT11) ? sizeof(*gSHT11_Obj) : 0;
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
		  + sizeOfOwT()
		  + sizeOfSCD41()
		  + sizeOfSHT11();

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
	ptr += gOwT_count * sizeof(OwT_Obj);

	gOwTempSensors = (ow::RomData*)ptr;
	ptr += sizeOfOwT();

	gSCD41_Obj = (SCD41_Obj*)ptr;
	ptr += sizeOfSCD41();

	gSHT11_Obj = (SHT11_Obj*)ptr;

	if(gFeatures & eFeatureSHT11){
		sht11::enable();
	}

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

namespace meas{
enum class State {
	idling,
	ow_initing_before_convert,
	ow_requesting_conver,
	ow_converting,
	ow_initing_before_read,
	ow_matching_rom,
	ow_reading_scratchpad,
	ow_relaxing
};

static auto gState = State::idling;
static uint8_t gT0;
static uint8_t gOwCurSensorIdx;

void ow_error()
{
	DBG("ow_error %u", ow::response());
	auto newValue = ow::cInvalidValue;
	if(not (gOwCurSensorIdx < gOwT_count))
		gOwCurSensorIdx = 0;
	auto& obj = gOwT_Obj[gOwCurSensorIdx];
	auto& curValue = *reinterpret_cast<int16_t*>(&gIOData[obj.iodataIdx]);
	if(curValue != newValue){
		curValue = newValue;
		markMobTx(obj.mobIdx);
	}
	gT0 = ClockTimer65MS::now();
	gState = State::ow_relaxing;
}

void measure_ow_temp_sm()
{
	static uint16_t gTmpOwMobMarkedTx;

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
		else if(ow::response() != ow::ResponseCode::eBusy){
			ow_error();
		}
		break;
	case State::ow_requesting_conver:
		if(ow::response() == ow::ResponseCode::eOwWriteBitsOk){
			gState = State::ow_converting;
			gT0 = ClockTimer65MS::now();
		}
		else if(ow::response() != ow::ResponseCode::eBusy){
			ow_error();
		}
		break;
	case State::ow_converting:
	{
		auto timeout = ClockTimer65MS::durMs<850>();
		if(ClockTimer65MS::isTimeout(gT0, timeout)){
			gOwCurSensorIdx = 0;
			gTmpOwMobMarkedTx = 0;
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
		else if(ow::response() != ow::ResponseCode::eBusy){
			ow_error();
		}
		break;
	case State::ow_matching_rom:
		if(ow::response() == ow::ResponseCode::eOwWriteBitsOk){
			ow::read_bits(sizeof(ow::ThermScratchpad)*8);
			gState = State::ow_reading_scratchpad;
		}
		else if(ow::response() != ow::ResponseCode::eBusy){
			ow_error();
		}
		break;
	case State::ow_reading_scratchpad:
		if(ow::response() == ow::ResponseCode::eOwReadBitsOk){
			auto spad = reinterpret_cast<const ow::ThermScratchpad*>(ow::gOwData);
			uint8_t crc = 0;
			for(uint8_t i=0; i<sizeof(*spad)-1; ++i){
				crc = ow::calc_crc(ow::gOwData[i], crc);
			}
			auto newValue = ow::cInvalidValue;
			if(crc == spad->crc){
				newValue = spad->temperature;
			}
			auto& obj = gOwT_Obj[gOwCurSensorIdx++];
			auto& curValue = *reinterpret_cast<int16_t*>(&gIOData[obj.iodataIdx]);
			if(curValue != newValue){
				curValue = newValue;
				gTmpOwMobMarkedTx |= _BV(obj.mobIdx);
			}

			if(gOwCurSensorIdx < gOwT_count){
				ow::init();
				gState = State::ow_initing_before_read;
			}
			else{
				gState = State::ow_relaxing;
				gMobMarkedTx |= gTmpOwMobMarkedTx;
			}
		  }
		  else if(ow::response() != ow::ResponseCode::eBusy){
			  ow_error();
		  }
		  break;
		case State::ow_relaxing:
		{
			auto timeout = ClockTimer65MS::durMs<1000>();
			if(ClockTimer65MS::isTimeout(gT0, timeout)){
				gState = State::idling;
			}
		}
			break;
		default:
			break;
		}
}
}

namespace {

uint8_t reverse_bits(uint8_t v)
{
    auto r = v;
    int s = 7;
    for(v >>= 1; v; v >>= 1) {
        r <<= 1;
        r |= v & 1;
        s--;
    }
    r <<= s;
    return r;
}

uint8_t calc_crc_8bit(uint8_t crc_prev, uint8_t byte)
{
    for(int bit = 7; bit >= 0; --bit) {
        auto new_bit = byte >> 7;
        byte <<= 1;
        auto crc_bit7 = crc_prev >> 7;
        auto differ = new_bit ^ crc_bit7;
        if(differ) {
            const uint8_t mask2 = (differ << 3) | (differ << 4);
            const uint8_t mask = (1 << 3) | (1 << 4);
            auto tmp_crc = crc_prev ^ mask2;
            crc_prev &= ~mask;
            crc_prev |= tmp_crc & mask;
        }
        crc_prev <<= 1;
        crc_prev |= differ;
    }
    return crc_prev;
}
}

namespace sht11 {

constexpr uint8_t cmdMeasTemp = 0b00000011;
constexpr uint8_t cmdMeasRH = 0b00000101;
uint8_t t0 = 0;

template<unsigned dur>
bool timeout()
{
	return ClockTimer65MS::isTimeout(t0, ClockTimer65MS::durMs<dur>());
}

void reset_time()
{
    t0 = ClockTimer65MS::now();
}

constexpr auto timeout5s = 5000u;
constexpr auto timeout500ms = 500u;

enum class StateL2 {
    Disabled,
    Relax,
    MeasT_Init,
    MeasT_SendCmd,
    MeasT_ReadH,
    MeasT_ReadL,
    MeasT_ReadCRC,
    MeasRH_Init,
    MeasRH_SendCmd,
    MeasRH_ReadH,
    MeasRH_ReadL,
    MeasRH_ReadCRC,
    ClearError
};

static auto state = StateL2::Disabled;
static uint16_t sTemp;
static uint16_t sRH;

void update_pvs()
{
	if(gSHT11_Obj->valueT() != sTemp
	|| gSHT11_Obj->valueRH() != sRH){
		gSHT11_Obj->valueT() = sTemp;
		gSHT11_Obj->valueRH() = sRH;
		markMobTx(gSHT11_Obj->mobIdx);
	}
}

bool eval_state_helper()
{
    auto stateL1 = do_smL1();
    if(stateL1 == State::Idle) {
        return true;
    } else if(stateL1 == State::Error) {
        reset_time();
        state = StateL2::Relax;
    }
    return false;
}

void enableL2()
{
	state = StateL2::Relax;
}

void do_sm()
{
    switch(state) {
    case StateL2::Disabled:
        break;
    case StateL2::Relax:
        if(timeout<timeout5s>()) {
            auto stateL1 = do_smL1();
            if(stateL1 != State::Idle) {
                reset();
                state = StateL2::ClearError;
            } else {
                init();
                state = StateL2::MeasT_Init;
            }
        }
        break;
    case StateL2::MeasT_Init:
        if(eval_state_helper()) {
            sendByte(cmdMeasTemp);
            reset_time();
            state = StateL2::MeasT_SendCmd;
        }
        break;
    case StateL2::MeasT_SendCmd: {
        auto stateL1 = do_smL1();
        if(stateL1 == State::Idle) {
            recvByte();
            state = StateL2::MeasT_ReadH;
        } else if(stateL1 == State::Error) {
            reset_time();
            state = StateL2::Relax;
        } else if(timeout<timeout500ms>()) {
            reset();
            state = StateL2::ClearError;
        }
    } break;
    case StateL2::MeasT_ReadH:
        if(eval_state_helper()) {
            sTemp = (uint16_t)getByte() << 8;
            recvByte();
            state = StateL2::MeasT_ReadL;
        }
        break;
    case StateL2::MeasT_ReadL:
        if(eval_state_helper()) {
            sTemp |= (uint16_t)getByte();
            recvByte(false);
            state = StateL2::MeasT_ReadCRC;
        }
        break;
    case StateL2::MeasT_ReadCRC:
        if(eval_state_helper()) {
            uint8_t crc = calc_crc_8bit(0, cmdMeasTemp);
            crc = calc_crc_8bit(crc, sTemp >> 8);
            crc = calc_crc_8bit(crc, sTemp & 0xFF);
            crc = reverse_bits(crc);
            if(crc == getByte()) {
                reset_time();
                init();
                state = StateL2::MeasRH_Init;
            } else {
                reset();
                state = StateL2::ClearError;
            }
        }
        break;
        ///////////////////////
    case StateL2::MeasRH_Init:
        if(eval_state_helper()) {
            sendByte(cmdMeasRH);
            reset_time();
            state = StateL2::MeasRH_SendCmd;
        }
        break;
    case StateL2::MeasRH_SendCmd: {
        auto stateL1 = do_smL1();
        if(stateL1 == State::Idle) {
            recvByte();
            state = StateL2::MeasRH_ReadH;
        } else if(stateL1 == State::Error) {
            reset_time();
            state = StateL2::Relax;
        } else if(timeout<timeout500ms>()) {
            reset();
            state = StateL2::ClearError;
        }
    } break;
    case StateL2::MeasRH_ReadH:
        if(eval_state_helper()) {
            sRH = (uint16_t)getByte() << 8;
            recvByte();
            state = StateL2::MeasRH_ReadL;
        }
        break;
    case StateL2::MeasRH_ReadL:
        if(eval_state_helper()) {
            sRH |= (uint16_t)getByte();
            recvByte(false);
            state = StateL2::MeasRH_ReadCRC;
        }
        break;
    case StateL2::MeasRH_ReadCRC:
        if(eval_state_helper()) {
            uint8_t crc = calc_crc_8bit(0, cmdMeasRH);
            crc = calc_crc_8bit(crc, sRH >> 8);
            crc = calc_crc_8bit(crc, sRH & 0xFF);
            crc = reverse_bits(crc);
            if(crc == getByte()) {
                reset_time();
                update_pvs();
                state = StateL2::Relax;
            } else {
                reset();
                state = StateL2::ClearError;
            }
        }
        break;
        /////////////////////
    case StateL2::ClearError: {
    	sRH = sTemp = Sensorion::cInvalidValue;
    	update_pvs();
        auto stateL1 = do_smL1();
        if(stateL1 == State::Idle || stateL1 == State::Error) {
            reset_time();
            state = StateL2::Relax;
        }
    } break;
    default:
        break;
    }
}
}
