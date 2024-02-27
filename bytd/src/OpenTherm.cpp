#include "OpenTherm.h"
#include "IMqttPublisher.h"
#include "Log.h"
#include "Pru.h"
#include "pru/rpm_iface.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace {

constexpr uint32_t calc_parity(unsigned v, unsigned pos);

constexpr uint32_t calc_parity(unsigned v, unsigned pos)
{
    if(pos == 0)
        return v & 1;

    return ((v & (1 << pos)) ? 1 : 0)
        + calc_parity(v, pos - 1);
}

constexpr uint32_t parity(uint32_t v)
{
    int par = 0;
    for(int i = 0; i < 31; ++i) {
        if(v & (1 << i))
            ++par;
    }

    if(par & 1)
        return v | (1 << 31);
    else
        return v & ~(1 << 31);
}

constexpr uint16_t float2f88(float v)
{
    return std::lround(v * 256);
}

constexpr float floatFromf88(uint16_t v)
{
    return v / 256.0;
}

std::string frameToStr(opentherm::Frame f)
{
    std::ostringstream ss;
    ss << '(' << opentherm::msg::msgToStr(f.getType())
       << '-' << f.getId()
       << "-/" << floatFromf88(f.getV())
       << "/0x" << std::hex << f.getV()
       << ')';
    return ss.str();
}

}

// Mrd-Srdack id=0 M: v16=03CA (00000011.11001010) f88=3.78906 S: v16=0320 (00000011.00100000) f88=3.125
// Mwr2-Swrack id=56 M: v16=2700 (00100111.00000000) f88=39.0 S: v16=2700 (00100111.00000000) f88=39.0
// Mwr-Swrack id=1 M: v16=3900 (00111001.00000000) f88=57.0 S: v16=3900 (00111001.00000000) f88=57.0
OpenTherm::OpenTherm(std::shared_ptr<Pru> pru, IMqttPublisherSPtr mqtt)
    : pru(pru)
    , rxMsg(std::make_shared<PruRxMsg>())
    , mqtt(mqtt)
    , asyncReq(opentherm::Frame::invalid)
    , tCH("tCH", mqtt)
    , tDHW("tDHW", mqtt)
{
    using opentherm::Frame;
    Flame = [](bool) {};
    DhwActive = [](bool) {};
    pru->setOtCbk(rxMsg);

    thrd = std::thread([this] {
        int id = 0;
        float dhw = 0, ch = 0, rddhw = 0, rdch = 0;
        while(not shutdown) {
            auto lastTransmit = std::chrono::steady_clock::now();
            if(id == 0) {
                // Mrd-Srdack id=0 M: v16=03CA (00000011.11001010) f88=3.78906 S: v16=0320 (00000011.00100000) f88=3.125
                uint32_t txv = 0x00CA | static_cast<uint16_t>(mode) << 8;
                auto rsp = transmit(txv);
                if(Frame(rsp).isValid()) {
                    uint16_t v = 0xFFFF & rsp;
                    if(v != status) {
                        publish_status(v);
                        status = v;
                        LogDBG("ot transfer id0 req:({:04X}){} rsp:{:b} {} ", txv, frameToStr(opentherm::Frame(txv)), status, frameToStr(opentherm::Frame(status)));
                    }
                }
                if(asyncReq.isValid()) {
                    Frame arsp = transmit(asyncReq.data);
                    if(arsp.isValid()) {
                        auto rspstr = frameToStr(arsp);
                        LogDBG("ot req:{} repsonse:{}", frameToStr(asyncReq), rspstr);
                        rspstr = "otTransfer " + rspstr;
                        this->mqtt->publish(mqtt::rbResponse, rspstr, false);
                    }
                    asyncReq = Frame::invalid;
                }
            } else if(id == 1) {
                // Mwr-Swrack id=1 M: v16=3900 (00111001.00000000) f88=57.0 S: v16=3900 (00111001.00000000) f88=57.0
                uint32_t txv = float2f88(chSetpoint);
                txv |= 1 << 16;
                txv |= 0b00010000 << 24;
                auto rsp = transmit(txv);
                if(Frame(rsp).isValid()) {
                    auto v = floatFromf88(rsp & 0xFFFF);
                    if(v != ch) {
                        ch = v;
                        LogDBG("ot transfer id1 {}", ch);
                    }
                }
            } else if(id == 2) {
                // Mwr-Swrack id=1 M: v16=3900 (00111001.00000000) f88=57.0 S: v16=3900 (00111001.00000000) f88=57.0
                // Mwr2-Swrack id=56 M: v16=2700 (00100111.00000000) f88=39.0 S: v16=2700 (00100111.00000000) f88=39.0
                uint32_t txv = float2f88(dhwSetpoint);
                txv |= 56 << 16;
                txv |= 0b00011000 << 24;
                auto rsp = transmit(txv);
                if(Frame(rsp).isValid()) {
                    auto v = floatFromf88(rsp & 0xFFFF);
                    if(v != dhw) {
                        dhw = v;
                        LogDBG("ot transfer id56 {}", dhw);
                    }
                }
            } else if(id == 3) {
                Frame txv(opentherm::msg::type::Mrd, 25, 0);
                Frame rsp(transmit(txv.data));
                if(rsp.isValid()) {
                    auto v = floatFromf88(rsp.getV());
                    if(v != rddhw) {
                        rddhw = v;
                        auto fv = floatFromf88(rsp.getV());
                        LogDBG("id{} cur ch:{}", rsp.getId(), fv);
                        tCH.setValue(fv);
                    }
                }
            } else if(id == 4) {
                Frame txv(opentherm::msg::type::Mrd, 26, 0);
                Frame rsp(transmit(txv.data));
                if(rsp.isValid()) {
                    auto v = floatFromf88(rsp.getV());
                    if(v != rdch) {
                        rdch = v;
                        auto fv = floatFromf88(rsp.getV());
                        LogDBG("id{} cur dhw:{}", rsp.getId(), fv);
                        tDHW.setValue(fv);
                    }
                }
            }

            if(++id > 4)
                id = 0;

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

void OpenTherm::publish_status(uint16_t newstat)
{
    const uint16_t changed = newstat ^ status;
    for(auto bit : { 0, 1, 2, 3 }) {
        const uint16_t mask = 1 << bit;
        if(changed & mask) {
            const bool v = newstat & mask;
            switch(bit) {
            case 0:
                mqtt->publish("rb/stat/ot/fault", v);
                break;
            case 1:
                mqtt->publish("rb/stat/ot/ch", v);
                break;
            case 2:
                DhwActive(v);
                mqtt->publish("rb/stat/ot/dhw", v);
                break;
            case 3:
                Flame(v);
                mqtt->publish("rb/stat/ot/flame", v);
                break;
            default:
                break;
            }
        }
    }
}

uint32_t OpenTherm::transmit(uint32_t frame)
{
    rxMsg->checkClear();
    using opentherm::Frame;
    uint32_t data[2] = { pru::eCmdOtTransmit, parity(frame) };
    pru->send((uint8_t*)&data, sizeof(data));
    auto buf = rxMsg->wait(std::chrono::seconds(4));

    if(buf.empty()) {
        LogERR("OpenTherm buf empty");
        return Frame::invalid;
    }

    if(buf.size() != sizeof(uint32_t)
        && buf.size() != 2 * sizeof(uint32_t)) {
        LogERR("OpenTherm buf invalid size {}", buf.size());
        return Frame::invalid;
    }

    auto pmsg = reinterpret_cast<uint32_t*>(&buf[0]);

    switch((pru::ResponseCode)pmsg[0]) {
    case pru::ResponseCode::eOtBusError:
        LogERR("opentherm bus error");
        break;
    case pru::ResponseCode::eOtNoResponse:
        LogERR("ot no response");
        break;
    case pru::ResponseCode::eOtFrameError: {
        auto timeout = std::numeric_limits<float>::quiet_NaN();
        uint32_t u = 0;
        if(buf.size() == 2 * sizeof(uint32_t)) {
            u = pmsg[1];
            timeout = u * 5e-6;
        }
        LogERR("ot rx eOtFrameError timeout:({}) {}ms", u, timeout);
        break;
    }
    case pru::ResponseCode::eOtOk: {
        if(buf.size() != 2 * sizeof(uint32_t)) {
            LogERR("OpenTherm buf invalid size {}", buf.size());
            return Frame::invalid;
        }
        auto frame = pmsg[1];
        if(frame != parity(frame)) {
            LogERR("ot rx error parity");
            return Frame::invalid;
        }
        return frame;
    } break;
    default:
        LogERR("ot other rx error {}", pmsg[0]);
        break;
    }
    return Frame::invalid;
}

static_assert(parity(opentherm::Frame::invalid) == opentherm::Frame::invalid, "Zly vyber konstanty");
