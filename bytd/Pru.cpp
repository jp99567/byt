/*
 * Pru.cpp
 *
 *  Created on: Dec case pru::ResponseCode::9: 2019
 *      Author: j
 */

#include "Pru.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory>

#include "Log.h"
#include "../pru/rpm_iface.h"

void PruRxMsg::rx(Buf buf)
{
  	{
  		std::lock_guard<std::mutex> guard(lck);
   		if(not buf.empty()){
   			LogERR("PruRxMsg drop buf");
   		}
   		this->buf = std::move(buf);
   	}
   	cv.notify_one();
}


void Pru::send(const uint8_t* data, std::size_t len)
{
	auto rv = ::write(fd, data, len);

	if(rv != sizeof(len)){
		LogSYSERR("pru write");
	}
}

Pru::Pru() {

	fd = ::open("/dev/rpmsg_pru30", O_RDWR);
	if( fd < 0 ) LogSYSDIE;

	thrd = std::thread([this]{
			while( not (fd<0) ){
				auto buf = PruRxMsg::Buf(32);
				auto len = ::read(fd, buf.data(), buf.size());
				if(len <= 0){
					LogSYSERR("pru read");
					break;
				}
				else{
					buf.resize(len);
					if(len < 4){
						LogERR("invalid pru msg size:{}", len);
						continue;
					}

					auto code = *reinterpret_cast<pru::ResponseCode*>(&buf[0]);

					auto send = [&buf](auto& wprx){
						auto prx = wprx.lock();
						if(prx){
							prx->rx(std::move(buf));
						}
					};

					switch(code)
					{
					case pru::ResponseCode::eRspError:
						LogERR("pru msg eRspError");
						break;
					case pru::ResponseCode::eOwPresenceOk:
					case pru::ResponseCode::eOwBusFailure0:
					case pru::ResponseCode::eOwBusFailure1:
					case pru::ResponseCode::eOwNoPresence:
					case pru::ResponseCode::eOwBusFailureTimeout:
					case pru::ResponseCode::eOwReadBitsOk:
					case pru::ResponseCode::eOwReadBitsFailure:
					case pru::ResponseCode::eOwWriteBitsOk:
					case pru::ResponseCode::eOwWriteBitsFailure:
					case pru::ResponseCode::eOwSearchResult0:
					case pru::ResponseCode::eOwSearchResult1:
					case pru::ResponseCode::eOwSearchResult11:
					case pru::ResponseCode::eOwSearchResult00:
						send(owRxMsg);
						break;
					case pru::ResponseCode::eOtFrameError:
					case pru::ResponseCode::eOtBusError:
					case pru::ResponseCode::eOtOk:
						send(otRxMsg);
						break;
					default:
						LogERR("pru msg unknown {}", (int)code);
						break;
					}
				}
			}
	});
}

Pru::~Pru() {
	auto tmpfd = fd;
	fd = -1;
	if(tmpfd >= 0)
		::close(tmpfd);

	thrd.join();
}

