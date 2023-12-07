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
#include <csignal>

#include "Log.h"
#include "pru/rpm_iface.h"
#include "thread_util.h"


void PruRxMsg::rx(Buf buf)
{
  	{
  		std::lock_guard<std::mutex> guard(lck);
   		if(not this->buf.empty()){
   			LogERR("PruRxMsg drop buf");
   		}
   		this->buf = std::move(buf);
   	}
    cv.notify_one();
}

void PruRxMsg::checkClear()
{
    std::lock_guard<std::mutex> guard(lck);
    if(not this->buf.empty()){
        LogERR("unexpected PruRxMsg drop buf");
        buf.clear();
    }
}


void Pru::send(const uint8_t* data, std::size_t len)
{
#ifndef BYTD_SIMULATOR
	auto rv = ::write(fd, data, len);

	if((rv < 0) || ((unsigned)rv != len)){
		LogSYSERR("pru write");
        }
#else
  PruRxMsg::Buf buf(1);
  using namespace pru;
  switch(*reinterpret_cast<const uint32_t*>(data)){
  case eCmdOwInit:
  case eCmdOwSearchDir0:
  case eCmdOwSearchDir1:
  case eCmdOwWrite:
  case eCmdOwRead:
  case eCmdOwWritePower:
    buf[0] = eOwBusFailure0;
break;
      case eCmdOtTransmit:
    buf[0] = eOtNoResponse;
    break;
    default:
        throw std::runtime_error("pru simulator");
      break;
  }
  std::ignore = len;
  respond(buf);
#endif
}

Pru::Pru() {

#ifndef BYTD_SIMULATOR
	fd = ::open("/dev/rpmsg_pru30", O_RDWR);
    if( fd < 0 ) LogSYSDIE("open pru");
#endif

	thrd = std::thread([this]{
		thread_util::set_thread_name("bytd-pru");
			while( not (fd<0) ){
				auto buf = PruRxMsg::Buf(32);
				auto len = ::read(fd, buf.data(), buf.size());
				if(len <= 0){
                    if(not(fd == -1 && errno == EINTR)){
                        LogSYSERR("pru read");
                    }
					break;
				}
				else{
					if(len < 4){
						LogERR("invalid pru msg size:{}", len);
						continue;
					}
                    buf.resize(len);

                                        respond(std::move(buf));
				}
			}
	});
}

Pru::~Pru() {
    auto tmpfd = fd;
    fd = -1;
    if(tmpfd >= 0){
      const int32_t cmd = pru::Commands::eCmdOwInit;
      ::write(tmpfd, (const uint8_t*)&cmd, sizeof(cmd)); //unblock blocking read
    }
    thrd.join();
    ::close(tmpfd);
    LogDBG("~Pru");
}

void Pru::respond(const PruRxMsg::Buf buf) {
  auto code = *reinterpret_cast<const pru::ResponseCode*>(&buf[0]);

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
  case pru::ResponseCode::eOtNoResponse:
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
