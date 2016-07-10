
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
#include "log_trace_debug.h"
#include "owtherm.h"
#include "exporter.h"
#include "thread_util.h"

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
	explicit OwThermNet(Exporter& e);
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
	void write_simple_cmd(Cmd cmd);
	int search_triplet(bool branch_direction);
	int mFd;
	std::vector<Sensor> mMeas;
	Exporter& mExporter;
};

int OwThermNet::search_triplet(bool branch_direction)
{
	int tmp(branch_direction);
	int err = ioctl(mFd, OWTHERM_IOCTL_SERCH_TRIPLET, &tmp);
	if(err)	SYSDIE;
	return tmp;
}

void OwThermNet::write_simple_cmd(Cmd cmd)
{
	if( 1 != write(mFd, &cmd, 1)) SYSDIE;
}

bool OwThermNet::measure()
{
	if(!presence()){
		ERRORS("no presence");
		return false;
	}

	write_simple_cmd(Cmd::SKIP_ROM);

	int tmp(1);
	if(ioctl(mFd, OWTHERM_IOCTL_STRONG_PWR, &tmp)) SYSDIE;

	write_simple_cmd(Cmd::CONVERT);
	std::this_thread::sleep_for(std::chrono::milliseconds(750));

	bool rv(true);
	ThermScratchpad res;
	constexpr RomCode mrc({0x28,{0x7F,0xC8,0x0A,0x06,0,0},0x74});
	// 287FC80A06000074

	Sample sample;
	for(auto& i: mMeas){

		float t;
		if(read_scratchpad(i.romcode, res)){
			t = res.temperature / 16.0;

			//check 0550 85 deg - conversion failed
			if( (uint16_t)res.temperature == 0x0550 &&
				abs(i.lastValid-85) > 1.5){
				ERRORS("invalid value 85");
				t = badval;
			}

			if(t > 125 || t < -55){
				ERRORS("out of range:%f", t);
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
	mExporter.put(std::move(sample));
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
			ERRORS("no sensor on the bus");
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
				ERRORS("search failed, b00 expected");
				return;
			}

			bool newval(dir);
			if(res==0){
				if(!newval){
					last_zero = bitidx;
				}
			}
			else if(res == 3){
				ERRORS("search failed, b11");
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
			ERRORS("search: crc error");
		}
	}
	while(last_branch >= 0);

	mMeas = std::move(tmp);
}

bool OwThermNet::presence()
{
	int tmp;
	int err = ioctl(mFd, OWTHERM_IOCTL_PRESENCE, &tmp);
	if(err)	SYSDIE;

	switch(tmp){
	case 0:
	case OWTHERM_ERR_NO_PRESENCE:
		break;
	case OWTHERM_ERR_BUS_INVALID:
		std::cerr << "OWTHERM_ERR_BUS_INVALID" << '\n';
		break;
	case OWTHERM_ERR_BUS_SHORT:
		std::cerr << "OWTHERM_ERR_BUS_SHORT" << '\n';
		break;
	case OWTHERM_ERR_TIMING:
		std::cerr << "OWTHERM_ERR_TIMING" << '\n';
		break;
	default:
		std::cerr << "UNKNOWN" << '\n';
		break;
	}

	return !tmp;
}

bool OwThermNet::read_scratchpad(const RomCode& rc, ThermScratchpad& v)
{
	if(!presence()){
		ERRORS("no presence");
		return false;
	}

	write_simple_cmd(Cmd::MATCH_ROM);

	int n = write(mFd, &rc, sizeof(rc));
	if(n!=sizeof(rc)) SYSDIE;

	write_simple_cmd(Cmd::READ_SCRATCHPAD);

	if(sizeof(v) != read(mFd, &v, sizeof(v))) SYSDIE;

	if( (v.conf != 0x7F ) ||
		(v.reserved[0]!=0xFF) ||
		(v.reserved[2]!=0x10) ){
		ERRORS("scrratchpad: %02X %02X %02X %02X %02X",
				(uint8_t)v.alarmH, (uint8_t)v.alarmL, v.conf, v.reserved[0], v.reserved[2] );
		return false;
	}

	if(!check_crc(v)){
		ERRORS("scratchpad crc error");
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
	if(n!=sizeof(rc)) SYSDIE;

	if(!check_crc(rc)){
		std::cerr << "error crc\n";
		return false;
	}
	return true;
}

OwThermNet::OwThermNet(Exporter& e)
	:mExporter(e)
{
	mFd = open("/dev/owtherm",  O_RDWR);
	if( mFd < 0 ) SYSDIE;
}

OwThermNet::~OwThermNet()
{
	if(close(mFd)) SYSDIE;
}

} //namespace ow

static void load_read_predefined_romcodes(std::set<ow::RomCode>& s)
{
	std::ifstream config("predefined_romcodes.txt");
	if(!config.good()) SYSDIE;
	config.exceptions(std::ifstream::badbit);
	std::string tmp;

	ow::RomCode tmprc;
	while(config.good()){
		if(config >> tmp && tmp[0]!='#'){
			tmprc = tmp;
			if(!s.insert(tmprc).second){
				DIES("duplicates:%s", tmp.c_str());
			}
		}
	}
}

int main()
{
	std::set<ow::RomCode> predefined;
	load_read_predefined_romcodes(predefined);

	if(predefined.empty()) DIES("no predefined sensors");

	ow::Exporter exporter;
	ow::OwThermNet therm(exporter);

	bool search_success(false);
	int try_nr(10);
	do{
		std::set<ow::RomCode> found;
		using std::begin;
		using std::end;

		therm.search();
		bool goto_break(false);
		for(auto& i: therm.measurment()){
			if(!found.insert(i.romcode).second){
				ERRORS("duplicate");
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
				ERRORS("missing sensors:");
				for(auto i=begin(tmpv); i!=it; ++i){
					std::cerr << '\t' << (std::string)*i << '\n';
				}
			}

			it = std::set_difference(begin(found), end(found),
									 begin(predefined), end(predefined),
									 begin(tmpv));

			if(it!=begin(tmpv)){
				ERRORS("new sensors:");
				for(auto i=begin(tmpv); i!=it; ++i){
					std::cerr << '\t' << (std::string)*i << '\n';
				}
			}
		}
		sleep(1);
	}
	while(--try_nr > 0);

	if(!search_success)
		DIES("search failed");

	thread_util::set_sig_handler();
	thread_util::sigblock(false, true);

	INFO("measure start");
	auto sample_time = std::chrono::system_clock::now();
	while(!thread_util::shutdown_req){

		therm.measure();

		sample_time += std::chrono::seconds(5);
		if(sample_time < std::chrono::system_clock::now()){
			sample_time = std::chrono::system_clock::now();
		}
		std::this_thread::sleep_until(sample_time);
	}

	return 0;
}
