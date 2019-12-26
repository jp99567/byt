#include "OpenTherm.h"
#include <chrono>
#include <cmath>
#include "Log.h"
#include "Pru.h"
#include "../pru/rpm_iface.h"
#include "opentherm_protocol.h"
#include "thread_util.h"

// Mrd-Srdack id=0 M: v16=03CA (00000011.11001010) f88=3.78906 S: v16=0320 (00000011.00100000) f88=3.125
// Mwr2-Swrack id=56 M: v16=2700 (00100111.00000000) f88=39.0 S: v16=2700 (00100111.00000000) f88=39.0
// Mwr-Swrack id=1 M: v16=3900 (00111001.00000000) f88=57.0 S: v16=3900 (00111001.00000000) f88=57.0
OpenTherm::OpenTherm(std::shared_ptr<Pru> pru)
	:pru(pru)
	,rxMsg(std::make_shared<PruRxMsg>())
{
	pru->setOtCbk(rxMsg);

	thrd = std::thread([this]{
            thread_util::set_thread_name("bytd-opentherm");
			while(not shutdown){
				auto lastTransmit = std::chrono::steady_clock::now();

				for(auto& p : rdParams){
				    auto rsp = Frame(transmit(Frame(opentherm::msg::type::Mrd, p.id, 0).data));
				    if(rsp.isValid()){
				        p.val = rsp.getV();
				    }
				}
				std::this_thread::sleep_until(lastTransmit + std::chrono::seconds(1));
			}
	});
}

OpenTherm::~OpenTherm()
{
	LogDBG("~OpenTherm");
	shutdown = true;
	thrd.join();
}

uint32_t OpenTherm::transmit(uint32_t frame)
{
	uint32_t data[2] = { pru::eCmdOtTransmit, parity(frame) };
	pru->send((uint8_t*)&data, sizeof(data));
	auto buf = rxMsg->wait(std::chrono::seconds(4));

	if(buf.empty()){
		LogERR("OpenTherm buf empty");
		return Frame::invalid;
	}

	if( buf.size() != sizeof(uint32_t)
	 && buf.size() != 2*sizeof(uint32_t)){
		LogERR("OpenTherm buf invalid size {}", buf.size());
		return Frame::invalid;
	}

	auto pmsg = reinterpret_cast<uint32_t*>(&buf[0]);

	switch((pru::ResponseCode)pmsg[0])
	{
	case pru::ResponseCode::eOtBusError:
	case pru::ResponseCode::eOtFrameError:
	case pru::ResponseCode::eOtNoResponse:
		break;
	case pru::ResponseCode::eOtOk:
	{
		if(buf.size() != 2*sizeof(uint32_t)){
			LogERR("OpenTherm buf invalid size {}", buf.size());
			return Frame::invalid;
		}
		auto frame = pmsg[1];
		if(frame != parity(frame)){
			LogERR("ot rx error parity");
			return Frame::invalid;
		}
		return frame;
	}
		break;
	default:
		break;
	}

	LogERR("ot rx error {}", pmsg[0]);
	return Frame::invalid;
}
