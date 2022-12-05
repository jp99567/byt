#include "OpenTherm.h"
#include <cstdint>
#include <chrono>
#include <cmath>
#include "Log.h"
#include "Pru.h"
#include "../pru/rpm_iface.h"

namespace opentherm
{
namespace msg
{
    enum class type
    {
        Mrd = 0b000'0000,
        Mwr = 0b001'0000,
        Minvalid = 0b010'0000,
        Mreserved = 0b011'0000,
        Srdack = 0b100'0000,
        Swrack = 0b101'0000,
        Sinvalid = 0b110'0000,
        Sunknown = 0b111'0000,
        Mwr2 = 0b001'1000
    };
}
}

namespace {

constexpr uint32_t calc_parity(unsigned v, unsigned pos);

constexpr uint32_t calc_parity(unsigned v, unsigned pos)
{
	if(pos==0)
		return v & 1;

	return ((v & (1<<pos)) ? 1 : 0)
	+ calc_parity(v, pos-1);
}

constexpr uint32_t parity(uint32_t v)
{
	int par=0;
	for(int i=0; i<31; ++i){
		if(v & (1<<i)) ++par;
	}

	if(par&1)
		return v | (1<<31);
	else
		return v & ~(1<<31);
}

constexpr uint16_t float2f88(float v)
{
	return std::lround(v * 256);
}

constexpr float floatFromf88(uint16_t v)
{
	return v / 256.0;
}

struct Frame
{
    void setType(opentherm::msg::type type)
    {
        reinterpret_cast<uint8_t*>(&data)[0] = (uint8_t)type;
    }

    opentherm::msg::type getType() const
    {
        return (opentherm::msg::type)(reinterpret_cast<const uint8_t*>(&data)[0]);
    }

    void setId(int id)
    {
        reinterpret_cast<uint8_t*>(&data)[1] = (uint8_t)id;
    }

    int getId() const
    {
        return (int)(reinterpret_cast<const uint8_t*>(&data)[1]);
    }

    void setV(uint16_t v)
    {
        reinterpret_cast<uint16_t*>(&data)[1] = (uint16_t)v;
    }

    uint16_t getV() const
    {
        return (reinterpret_cast<const uint16_t*>(&data)[1]);
    }

    Frame(opentherm::msg::type type, int id, uint16_t val)
    {
        setType(type);
        setId(id);
        setV(val);
    }

    Frame(uint32_t data)
    :data(data){}

    bool isValid() const { return data != invalid; }

    static constexpr uint32_t invalid = 0x7FFFFFFF;
    uint32_t data = invalid;
};

}

// Mrd-Srdack id=0 M: v16=03CA (00000011.11001010) f88=3.78906 S: v16=0320 (00000011.00100000) f88=3.125
// Mwr2-Swrack id=56 M: v16=2700 (00100111.00000000) f88=39.0 S: v16=2700 (00100111.00000000) f88=39.0
// Mwr-Swrack id=1 M: v16=3900 (00111001.00000000) f88=57.0 S: v16=3900 (00111001.00000000) f88=57.0
OpenTherm::OpenTherm(std::shared_ptr<Pru> pru)
	:pru(pru)
	,rxMsg(std::make_shared<PruRxMsg>())
{
	pru->setOtCbk(rxMsg);

	thrd = std::thread([this]{
		    int id=0;
			while(not shutdown){
				auto lastTransmit = std::chrono::steady_clock::now();
                if(id==0){
                    //Mrd-Srdack id=0 M: v16=03CA (00000011.11001010) f88=3.78906 S: v16=0320 (00000011.00100000) f88=3.125
                    uint32_t txv = 0x03CA;
                    auto rsp = transmit(txv);
                    LogINFO("ot transfer id0 {:b}", 0xFFFF&rsp);
                }
                else if(id==1){
                    // Mwr-Swrack id=1 M: v16=3900 (00111001.00000000) f88=57.0 S: v16=3900 (00111001.00000000) f88=57.0
                    uint32_t txv = float2f88(chSetpoint);
                    txv |= 1 << 16;
                    txv |= 0b00010000 << 24;
                    auto rsp = transmit(txv);
                    LogINFO("ot transfer id1 {}", floatFromf88(rsp&0xFFFF));
                }
                else if(id==2){
                    // Mwr-Swrack id=1 M: v16=3900 (00111001.00000000) f88=57.0 S: v16=3900 (00111001.00000000) f88=57.0
                    // Mwr2-Swrack id=56 M: v16=2700 (00100111.00000000) f88=39.0 S: v16=2700 (00100111.00000000) f88=39.0
                    uint32_t txv = float2f88(dhwSetpoint);
                    txv |= 56 << 16;
                    txv |= 0b00011000 << 24;
                    auto rsp = transmit(txv);
                    LogINFO("ot transfer id56 {}", floatFromf88(rsp&0xFFFF));
                }

                if(++id > 2)
                    id=0;

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
