
#include "therm.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstdint>
#include <system_error>
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <cstring>
#include <set>
#include <fstream>
#include "Log.h"
#include "owtherm.h"
#include "thread_util.h"
#include "../pru/rpm_iface.h"

namespace pru {

class Response
{
	const uint8_t* data;
	const std::size_t len;
	OwResponse code = eRspError;

public:
	Response(const uint8_t* data, std::size_t len)
	:data(data)
	,len(len)
{}

bool check()
{
	if( len < sizeof(int32_t))
	{
		LogERR("pru invalid msg len:{}", len);
		return false;
	}

	auto i = *reinterpret_cast<const int32_t*>(data);
	code = (OwResponse)i;

	switch(code)
	{
	  case eOwBusFailure0:
	  case eOwBusFailure1:
	  case eOwSearchResult00:
	  case eOwSearchResult0:
	  case eOwSearchResult1:
	  case eOwSearchResult11:
	  case eOwReadBitsFailure:
	  case eOwWriteBitsOk:
	  case eOwWriteBitsFailure:
		  if(len == sizeof(int32_t))
			  return true;
		  LogERR("pru msg unexpected size len:{} code:{}", len, code);
	    break;
	  case eOwPresenceOk:
	  case eOwNoPresence:
	  case eOwBusFailureTimeout:
		  if( len == 2*sizeof(int32_t) )
			  return true;
		  LogERR("pru msg unexpected size len:{} code:{}", len, code);
		break;
	  case eRspError:
		  LogERR("pru error msg. len:{}", len);
		break;
	  case eOwReadBitsOk:
		  if( len > sizeof(int32_t) )
			  return true;
		  LogERR("pru msg unexpected size len:{} code:{}", len, code);
		break;
	  default:
		  LogERR("pru unknown msg: {}({})", code, len);
		break;
	}

	return false;
}

int32_t getParam() const
{
	if(len < 2*sizeof(int32_t))
		return -1;

	return reinterpret_cast<const int32_t*>(data)[1];
}

OwResponse getCode() const
{
	return code;
}

std::string getCodeStr() const
{
	return getCodeStr(code);
}

static std::string getCodeStr(OwResponse code)
{
	switch(code){
    case eRspError:  return "eRspError";
    case eOwPresenceOk:  return "eOwPresenceOk";
    case eOwBusFailure0:  return "eOwBusFailure0";
    case eOwBusFailure1:  return "eOwBusFailure1";
    case eOwNoPresence:  return "eOwNoPresence";
    case eOwBusFailureTimeout:  return "eOwBusFailureTimeout";
    case eOwReadBitsOk:  return "eOwReadBitsOk";
    case eOwReadBitsFailure:  return "eOwReadBitsFailure";
    case eOwWriteBitsOk:  return "eOwWriteBitsOk";
    case eOwWriteBitsFailure:  return "eOwWriteBitsFailure";
    default:
    	return "???";
	}
}

};

}

namespace ow {

struct ThermScratchpad
{
	int16_t temperature;
	int8_t alarmH;
	int8_t alarmL;
	uint8_t conf;
	char reserved[3];
	uint8_t crc;
}__attribute__ ((__packed__));

class OwThermNet {
	enum class Cmd : uint8_t
	{
		READ_ROM = 0x33,
		CONVERT = 0x44,
		MATCH_ROM = 0x55,
		READ_SCRATCHPAD = 0xBE,
		SKIP_ROM = 0xCC,
		SEARCH = 0xF0,
	};

public:
	explicit OwThermNet(ExporterSptr e, const RomCode& mainSensor);
	~OwThermNet();

	struct Sensor {
		explicit Sensor(const RomCode& rc)
		:romcode(rc)
		,curVal(badval)
		,lastValid(badval){}

		//Sensor(const Sensor&) = delete;

		bool update(float v)
		{
			if(v==curVal)
				return false;
			curVal = v;

			if(v!=badval)
				lastValid = v;

			return true;
		}

		RomCode romcode;
		float curVal;
		float lastValid;
	};

	void search();
	bool measure();
	bool read_rom(RomCode& rc);
	bool read_scratchpad(const RomCode& rc, ThermScratchpad& v);

	const std::vector<Sensor>& measurment()
	{
		return mMeas;
	}

private:
	bool presence();
	void write_bits(const void* data, std::size_t bitlen, bool strong_pwr=false);
	void read_bits(std::size_t bitlen);
	void write_simple_cmd(Cmd cmd, bool strongpower=false);
	int search_triplet(bool branch_direction);

	int mFd;
	std::vector<Sensor> mMeas;
	ExporterSptr mExporter;
	const RomCode mrc;

	struct PruWriteMsg
	{
		struct {
		  uint32_t code;
		  uint32_t bitlen;
		} h;
		uint8_t data[24];
	};

	struct PruReadMsg
	{
		struct {
		  uint32_t code;
		} h;
		uint8_t data[28];
	};

	union {
		PruWriteMsg wr;
		PruReadMsg rd; } msg;
};

void OwThermNet::write_bits(const void* data, std::size_t bitlen, bool strong_pwr)
{
	msg.wr.h.code = strong_pwr ? pru::Commands::eCmdOwWritePower : pru::Commands::eCmdOwWrite;
	msg.wr.h.bitlen = bitlen;

	auto bytes = 1+(bitlen-1)/8;
	memcpy(msg.wr.data, data, bytes);
	auto rv = ::write(mFd, &msg, sizeof(msg.wr.h)+bytes);

	auto cleanup = [this](const char* msg)
		{
			LogSYSERR(msg);
			auto tmpFd = mFd;
			mFd = -1;
			::close(tmpFd);
			return;
		};

	if((std::size_t)rv != sizeof(msg.wr.h)+bytes)
	{
		cleanup("pru write failure");
		return;
	}

	rv = ::read(mFd, &msg, sizeof(msg));
	if(rv <= 0)
	{
		cleanup("pru read failure");
		return;
	}

	pru::Response rsp((uint8_t*)&msg, rv);

	if(not rsp.check())
	{
		return;
	}

	switch(rsp.getCode())
	{
	case pru::OwResponse::eOwWriteBitsOk:
		return;
	case pru::OwResponse::eOwWriteBitsFailure:
		LogERR("OwThermNet::write_bits failure");
		break;
	default:
		LogERR("OwThermNet::write_bits unknown failure");
		break;
	}
}

void OwThermNet::read_bits(std::size_t bitlen)
{
	msg.wr.h.code = pru::Commands::eCmdOwRead;
	msg.wr.h.bitlen = bitlen;

	auto rv = ::write(mFd, &msg, sizeof(msg.wr.h));

	auto cleanup = [this](const char* msg)
		{
			LogSYSERR(msg);
			auto tmpFd = mFd;
			mFd = -1;
			::close(tmpFd);
			return;
		};

	if(rv != sizeof(msg.wr.h))
	{
		cleanup("pru write failure");
		return;
	}

	rv = ::read(mFd, &msg.rd, sizeof(msg.rd));
	if(rv <= 0)
	{
		cleanup("pru read failure");
		return;
	}

	pru::Response rsp((uint8_t*)&msg, rv);

	if(not rsp.check())
	{
		return;
	}

	switch(rsp.getCode())
	{
	case pru::OwResponse::eOwReadBitsOk:
		return;
	case pru::OwResponse::eOwReadBitsFailure:
		LogERR("OwThermNet::read_bits failure");
		break;
	default:
		LogERR("OwThermNet::read_bits unknown failure");
		break;
	}
}

int OwThermNet::search_triplet(bool branch_direction)
{
	int32_t buf[32];
	buf[0] = branch_direction ? pru::Commands::eCmdOwSearchDir0 : pru::Commands::eCmdOwSearchDir1;
	auto rv = ::write(mFd, buf, 4);

	auto cleanup = [this](const char* msg)
		{
			LogSYSERR(msg);
			auto tmpFd = mFd;
			mFd = -1;
			::close(tmpFd);
			return 3;
		};

	if(rv != 4)
	{
		return cleanup("pru write failure");
	}

	rv = ::read(mFd, buf, sizeof(buf));
	if(rv <= 0)
	{
		return cleanup("pru read failure");
	}

	pru::Response rsp((uint8_t*)buf, rv);

	if(not rsp.check())
	{
		return 3;
	}

	int retval(3);

	switch(rsp.getCode())
	{
	case pru::OwResponse::eOwSearchResult00:
		retval = 0;
		break;
	case pru::OwResponse::eOwSearchResult0:
		retval = 1;
		break;
	case pru::OwResponse::eOwSearchResult1:
		retval = 2;
		break;
	case pru::OwResponse::eOwSearchResult11:
	default:
		retval = 3;
		break;
	}

	return retval;
}

void OwThermNet::write_simple_cmd(Cmd cmd, bool strongpower)
{
	write_bits(&cmd, 8, strongpower);
}

bool OwThermNet::measure()
{
	if(!presence()){
		LogERR("no presence");
		return false;
	}
return false;
	write_simple_cmd(Cmd::SKIP_ROM);
	write_simple_cmd(Cmd::CONVERT, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(750));

	bool rv(true);
	ThermScratchpad res;

	Sample sample;
	for(auto& i: mMeas){

		float t;
		if(read_scratchpad(i.romcode, res)){
			t = res.temperature / 16.0;

			//check 0550 85 deg - conversion failed
			if( (uint16_t)res.temperature == 0x0550 &&
				abs(i.lastValid-85) > 1.5){
				LogERR("invalid value 85");
				t = badval;
			}

			if(t > 125 || t < -55){
				LogERR("out of range:{}", t);
				t = badval;
			}
		}
		else{
			t = badval;
			rv = false;
		}

		if(i.update(t)){
			sample.add(i.romcode, t);

			////////////////
			if(t!=badval && i.romcode == mrc){
				FILE *f = fopen("/run/cur_temp", "w");
				fprintf(f, "%.4f\n", t);
				fclose(f);
			}
			///////////////
		}
	}

	if(sample.empty())
		return false;
	mExporter->put(std::move(sample));
	return rv;
}

void OwThermNet::search()
{
	int last_branch(-1);
	struct RomCode rc;
	uint8_t* const ptr = reinterpret_cast<uint8_t*>(&rc);
	std::vector<Sensor> tmp;

	mMeas.clear();
	do{
		if(!presence()){
			LogERR("no sensor on the bus");
			return;
		}

		write_simple_cmd(Cmd::SEARCH);

		int last_zero(-1);
		for(int bitidx = 0; bitidx<(int)sizeof(rc)*8; ++bitidx){
			int byte = bitidx/8, bit = bitidx%8;

			bool dir(0);
			bool expext_b00(false);
			if(bitidx == last_branch){
				dir = 1;
				expext_b00 = true;
			}
			else if(bitidx < last_branch)
				dir = ptr[byte] & (1<<bit);

			int res = search_triplet(dir);
			if(expext_b00 && res != 0){
				LogERR("search failed, b00 expected");
				return;
			}

			bool newval(dir);
			if(res==0){
				if(!newval){
					last_zero = bitidx;
				}
			}
			else if(res == 3){
				LogERR("search failed, b11");
				return;
			}
			else{
				newval = res == 1;
			}

			if(newval)
				ptr[byte] |= (1<<bit);
			else
				ptr[byte] &= ~(1<<bit);
		}
		last_branch = last_zero;

		if(check_crc(rc)){
			tmp.push_back(Sensor(rc));
		}
		else{
			LogERR("search: crc error");
		}
	}
	while(last_branch >= 0);

	mMeas = std::move(tmp);
}

bool OwThermNet::presence()
{
	int32_t buf[32];
	buf[0] = pru::Commands::eCmdOwInit;
	auto rv = ::write(mFd, buf, 4);

	auto cleanup = [this](const char* msg)
		{
			LogSYSERR(msg);
			auto tmpFd = mFd;
			mFd = -1;
			::close(tmpFd);
			return false;
		};

	if(rv != 4)
	{
		return cleanup("pru write failure");
	}

	rv = ::read(mFd, buf, sizeof(buf));
	if(rv <= 0)
	{
		return cleanup("pru read failure");
	}

	pru::Response rsp((uint8_t*)buf, rv);

	if(not rsp.check())
	{
		return false;
	}

	bool retval(false);

	switch(rsp.getCode())
	{
	case pru::eOwPresenceOk:
		LogDBG("presence:{}", rsp.getCodeStr());
		retval = true;
		break;
	default:
		LogERR("presence:{}", rsp.getCodeStr());
		break;
	}

	if(rsp.getParam() >= 0)
	{
		LogDBG("ow bus falling edge: {}us", 5e-3*rsp.getParam());
	}

	return retval;
}

bool OwThermNet::read_scratchpad(const RomCode& rc, ThermScratchpad& v)
{
	if(!presence()){
		LogERR("no presence");
		return false;
	}

	write_simple_cmd(Cmd::MATCH_ROM);
	write_bits(&rc, 8*sizeof(rc) );
	write_simple_cmd(Cmd::READ_SCRATCHPAD);
	read_bits(8*sizeof(v));
	memcpy(&v, msg.rd.data, sizeof(v));

	if( (v.conf != 0x7F ) ||
		(v.reserved[0]!=0xFF) ||
		(v.reserved[2]!=0x10) ){
		LogERR("scrratchpad: {:02X} {:02X} {:02X} {:02X} {:02X}",
				(uint8_t)v.alarmH, (uint8_t)v.alarmL, v.conf, v.reserved[0], v.reserved[2] );
		return false;
	}

	if(!check_crc(v)){
		LogERR("scratchpad crc error");
		return false;
	}

	return true;
}

bool OwThermNet::read_rom(RomCode& rc)
{
	if(!presence())
		return false;

	write_simple_cmd(Cmd::READ_ROM);

	int n = read(mFd, &rc, sizeof(rc));
	if(n!=sizeof(rc)) LogSYSDIE;

	if(!check_crc(rc)){
		std::cerr << "error crc\n";
		return false;
	}
	return true;
}

OwThermNet::OwThermNet(ExporterSptr e, const RomCode& mainSensor)
	:mExporter(e)
	,mrc(mainSensor)
{
	mFd = ::open("/dev/rpmsg_pru30", O_RDWR);
	if( mFd < 0 ) LogSYSDIE;
}

OwThermNet::~OwThermNet()
{
	if(close(mFd)) LogSYSDIE;
}

} //namespace ow

static void load_read_predefined_romcodes(std::set<ow::RomCode>& s, ow::RomCode& main)
{
	std::ifstream config("predefined_romcodes.txt");
	if(!config.good()) LogSYSDIE;
	config.exceptions(std::ifstream::badbit);
	std::string tmp;

	ow::RomCode tmprc;
	bool first = true;
	while(config.good()){
		if(config >> tmp && tmp[0]!='#'){
			tmprc = tmp;
			if(!s.insert(tmprc).second){
				LogDIE("duplicates:{}", tmp);
			}
			if(first){
				main = tmprc;
				first = false;
			}
		}
	}
}

MeranieTeploty::MeranieTeploty()
{
}

MeranieTeploty::~MeranieTeploty()
{
}

bool MeranieTeploty::init(ow::ExporterSptr exporter)
{
	std::set<ow::RomCode> predefined;
	ow::RomCode mainSensor;
	load_read_predefined_romcodes(predefined, mainSensor);

	if(predefined.empty()){
		LogERR("no predefined sensors");
	}

	auto therm = std::make_unique<ow::OwThermNet>(exporter, mainSensor);

	bool search_success(false);
	int try_nr(10);
	do{
		std::set<ow::RomCode> found;
		using std::begin;
		using std::end;

		therm->search();
		bool goto_break(false);
		for(auto& i: therm->measurment()){
			if(!found.insert(i.romcode).second){
				LogERR("duplicate");
				goto_break = true;
			}
		}
		if(goto_break)break;

		std::vector<ow::RomCode> tmpv(found.size()+predefined.size());

		if(found == predefined){
			search_success = true;
			break;
		}
		else{
			auto it = std::set_difference(begin(predefined), end(predefined),
							       	   	  begin(found), end(found),
										  begin(tmpv));

			if(it!=begin(tmpv)){
				for(auto i=begin(tmpv); i!=it; ++i){
					LogERR("missing sensor: {}", (std::string)*i);
				}
			}

			it = std::set_difference(begin(found), end(found),
									 begin(predefined), end(predefined),
									 begin(tmpv));

			if(it!=begin(tmpv)){
				for(auto i=begin(tmpv); i!=it; ++i){
					LogERR("new sensors: {}", (std::string)*i);
				}
			}
		}
		sleep(1);
	}
	while(--try_nr > 0);

	if(!search_success)
		LogERR("search failed");

	return search_success && not predefined.empty();
}

void MeranieTeploty::meas()
{
	therm->measure();
}
