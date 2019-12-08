#include <array>
#include <memory>
#include <chrono>
#include "spdlog/spdlog.h"
#include <unistd.h>
#include "spdlog/sinks/stdout_color_sinks.h"
#include <iostream>
#include <csignal>
#include <map>
#include <iomanip>

namespace {
std::shared_ptr<spdlog::logger> console;
spdlog::level::level_enum loglevel = spdlog::level::info;
}

namespace opentherm
{
namespace msg
{
    enum class type
    {
        Mrd = 0b0000,
        Mwr = 0b0010,
        Minvalid = 0b0100,
        Mreserved = 0b0110,
        Srdack = 0b1000,
        Swrack = 0b1010,
        Sinvalid = 0b1100,
        Sunknown = 0b1110,
		Mwr2 = 0b0011,
    };
}
}

std::map<unsigned, std::pair<uint32_t,uint32_t>> msgs;

namespace SayOt
{

uint8_t type(uint32_t v)
{
    return (v>>24) & 0x7F;
}

uint8_t id(uint32_t v)
{
    return (v>>16) & 0xFF;
}

uint16_t v16(uint32_t v)
{
    return v & 0xFFFF;
}

float f88(uint16_t v)
{
    return v / 256.0;
}
template< typename T >
std::string int_to_hex( T i )
{
  std::stringstream stream;
  stream << "0x"
         << std::setfill ('0') << std::setw(sizeof(T)*2)
         << std::hex << i;
  return stream.str();
}

std::string typeStr(uint8_t v)
{
    using opentherm::msg::type;

    std::string s;

    switch((type)(v >> 3))
    {
    case type::Mrd: s="Mrd"; break;
    case type::Mwr: s="Mwr"; break;
    case type::Mwr2: s="Mwr2"; break;
    case type::Minvalid: s="Minvalid"; break;
    case type::Mreserved: s="Mreserved"; break;
    case type::Srdack: s="Srdack"; break;
    case type::Swrack: s="Swrack"; break;
    case type::Sinvalid: s="Sinvalid"; break;
    case type::Sunknown: s="Sunknown"; break;
    default: s=std::string("X").append(std::to_string((unsigned)v)); break;
    }

    int spare = v & 0x07;
    if(spare){
        s.append(std::to_string(spare));
        console->error("non zero spare");
    }

    return s;
}

void sayOt(uint32_t mv, uint32_t sv)
{
    unsigned mid = id(mv);

    auto mbb = v16(mv);
    uint8_t mhb = mbb >> 8;
    uint8_t mlb = mbb & 0xFF;
    auto mf = f88(mbb);

    auto sbb = v16(sv);
    uint8_t shb = sbb >> 8;
    uint8_t slb = sbb & 0xFF;
    auto sf = f88(sbb);

    if( mid != id(sv)){
        console->error("msg id unmatch {}-{} id={} M: v16={:04X} ({:08b}.{:08b}) f88={} S: v16={:04X} ({:08b}.{:08b}) f88={}",
                typeStr(type(mv)), typeStr(type(sv)), mid,
    			mbb, mhb, mlb, mf,
    			sbb, shb, slb, sf
    			);
        return;
    }
    else{
    	auto prev = msgs.find(mid);

    	if(prev == std::cend(msgs)){
    		msgs.insert(std::make_pair(mid, std::make_pair(mv,sv)));
    	}
    	else if(prev->second.first == mv && prev->second.second == sv){
    		return;
    	}

    	prev->second = std::make_pair(mv,sv);

        console->info("{}-{} id={} M: v16={:04X} ({:08b}.{:08b}) f88={} S: v16={:04X} ({:08b}.{:08b}) f88={}",
                typeStr(type(mv)), typeStr(type(sv)), mid,
    			mbb, mhb, mlb, mf,
    			sbb, shb, slb, sf
    			);
    }
}

}

void test_adc()
{

	std::array<uint32_t,64> buf;
	buf.fill(0);
    
	auto fd = ::open("/dev/rpmsg_pru30", O_RDWR);

	if(fd == -1){
		console->error("open: {}", strerror(errno));
                return;
	}
		
	
	buf[0] = 1;
	if( 4 != ::write(fd, buf.data(), 4) ){
		console->error("1st write: {}", strerror(errno));
                return;
	}

	auto maxlen = buf.size()*sizeof(buf[0]);
	auto len = ::read(fd, buf.data(), maxlen);
	console->info("1st read({}) maxlen:{}", len, maxlen);

	int v_in = 0;
	uint32_t mv = 0, sv = 0;
	while(1){
		if( console->level() != loglevel){
			console->info("log level changed {}", (int)loglevel);
			console->set_level(loglevel);
		}
		len = ::read(fd, buf.data(), maxlen);
		if(len > 0){
			if(len == 16){
				if( buf[0] == 0x15AAABCD || buf[0] == 0x14AAABCD ){
					const uint32_t& v = buf[3];
					using ba4t = std::array<uint8_t,4>;
					const auto& ba = *reinterpret_cast<const ba4t*>(&buf[3]);
					int par=0;
					for(int i=0; i<31; ++i){
						if(v & (1<<i)) ++par;
					}

					if((par&1) != (v>>31)){
					    console->error("parity error");
					    continue;
					}
					uint16_t v16 = v & 0xFFFF;
					console->debug("msg{:X}({}) ({:08X} {}({:04X}) {:02X} {:02X} {:02X} {:02X}) type{:X}", buf[0]>>24, (par&1) == (v>>31),
						v, v16, v16, ba[3], ba[2], ba[1], ba[0], (ba[3]) & 0x7F );

					if( buf[0]>>24 == 0x15){
					    if( v_in != 0){
					        console->error("unexpected order0");
					        v_in = 0;
					        continue;
					    }
					    mv = v;
					    v_in++;
					}
					else{
					    if( v_in != 1){
					        console->error("unexpected order1");
					        v_in = 0;
					        continue;
					    }
					    sv = v;
					    v_in = 0;
					    SayOt::sayOt(mv, sv);
					}
				}
				else{
					console->error("read unknown frame({}) {:08X} {}ms {} {}", len, buf[0], buf[1]*5e-6, buf[2], buf[3]);
				}
			}
			else{
				console->error("read({})", len);
			}

		}
		else{
			if(errno == EINTR)
				continue;

			console->error("read error: {} {}", len, strerror(errno));
			break;
		}
	}

	::close(fd);
}

int main()
{
	std::signal(SIGUSR1,[](int){
			auto ltmp = (int)loglevel;
			if(++ltmp > (int)spdlog::level::err)
				loglevel = spdlog::level::trace;
			else
				loglevel = (spdlog::level::level_enum)ltmp;
	});
    console = spdlog::stdout_color_st("console");
    console->set_pattern("%L[%a %T %o] %v");
    console->set_level(spdlog::level::debug);
	test_adc();
	return 0;
}
