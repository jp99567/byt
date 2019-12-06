#include <array>
#include <memory>
#include <chrono>
#include "spdlog/spdlog.h"
#include <unistd.h>
#include "spdlog/sinks/stdout_color_sinks.h"
#include <iostream>

std::shared_ptr<spdlog::logger> console;

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

	while(1){
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
					uint16_t v16 = v & 0xFFFF;
					console->info("msg{:X}({}) ({:08X} {}({:04X}) {:02X} {:02X} {:02X} {:02X}) type{:X}", buf[0]>>24, (par&1) == (v>>31),
						v, v16, v16, ba[3], ba[2], ba[1], ba[0], (ba[3]) & 0x7F );
				}
				else{
					console->info("read({}) {:08X} {}ms {} {}", len, buf[0], buf[1]*5e-6, buf[2], buf[3]);
				}
			}
			else{
				console->info("read({})", len);
			}

		}
		else{
			console->error("read error: {} {}", len, strerror(errno));
			break;
		}


	}

	::close(fd);
}

int main()
{
    console = spdlog::stdout_color_st("console");
    console->set_pattern("[%a %T %o] %v");
	test_adc();
	return 0;
}
