/*
 * Elektromer.cpp
 *
 *  Created on: Jun 23, 2019
 *      Author: j
 */

#include "Elektromer.h"
#include "IMqttPublisher.h"
#include "Log.h"
#include "thread_util.h"
#include <gpiod.hpp>
#include <poll.h>

namespace {

constexpr double imp_per_kwh = 1000;

constexpr double calcPwr(std::chrono::milliseconds ms)
{
    auto hour = ms.count() / 3600e3;
    auto kw = 1 / (hour * imp_per_kwh);
    return kw * 1e3;
}

}

double Elektromer::getPowerCur() const
{
    if(lastPeriod == std::chrono::milliseconds::max())
        return 0.0;

    auto nt = Clock::now();

    auto d = nt - lastImp;

    if(d > lastPeriod) {
        return calcPwr(std::chrono::duration_cast<std::chrono::milliseconds>(d));
    }

    return calcPwr(lastPeriod);
}

double Elektromer::getKWh() const
{
    return impCount / imp_per_kwh;
}

void Elektromer::event(EventType e)
{
    Impulzy::event(e);

    if(e == EventType::rising) {
        auto nt = Clock::now();
        lastPeriod = std::chrono::duration_cast<std::chrono::milliseconds>(nt - lastImp);
        lastImp = nt;
    }

    auto pwr = getPowerCur();
    if(pwr != curPwr) {
        curPwr = pwr;
        mqtt.publish("rb/stat/power", std::to_string(pwr));
    }
}

float Elektromer::calc() const
{
    return orig + impCount / imp_per_kwh;
}

Elektromer::Elektromer(IMqttPublisher& mqtt)
    : Impulzy("1", 17, mqtt, "elektromer_total_kwh", gpiod::line::edge::RISING)
{
    threadName = "bytd-elektromer";
    svc_init();
}

Elektromer::~Elektromer()
{
    LogDBG("~Elektromer");
}

Impulzy::Impulzy(std::string chipname, unsigned int line, IMqttPublisher& mqtt,
    const char* filename, const gpiod::line::edge line_req_type, int timeout_ms)
    : ImpulzyBase(mqtt, filename)
    , chipName(chipname)
    , chipLine(line)
    , line_req_type(line_req_type)
    , timeout(std::chrono::nanoseconds((uint64_t)(1e6*timeout_ms)))
{
}

Impulzy::~Impulzy()
{
    active = false;
    t.join();
    LogINFO("~Impulzy joined");
}

void Impulzy::svc_init()
{
    t = std::thread([this] {
        thread_util::set_thread_name(threadName.c_str());

        std::string consumer_name = threadName + chipName + std::to_string(chipLine);
        try {
            auto request =
                ::gpiod::chip(chipName)
                    .prepare_request()
                    .set_consumer(consumer_name)
                    .add_line_settings(
                        chipLine,
                        ::gpiod::line_settings()
                            .set_direction(
                                ::gpiod::line::direction::INPUT)
                            .set_edge_detection(line_req_type)
                        )
                    .do_request();

            ::gpiod::edge_event_buffer buffer(1);

            LogINFO("impulze imp pin reqeusted: {} ({})", consumer_name, request.get_value(chipLine));

            struct pollfd pollfd;
            pollfd.fd = request.fd();
            pollfd.events = POLLIN;

            auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
            while(active) {
                auto ret = poll(&pollfd, 1, timeout_ms);
                if (ret == -1) {
                    LogERR("error waiting for edge events: {}", strerror(errno));
                    active = false;
                    break;
                }
                else if(ret == 0){
                    event(EventType::timeout);
                    continue;
                }

                request.read_edge_events(buffer);

                for (const auto& ev : buffer)
                {
                    event(ev.type() == gpiod::edge_event::event_type::RISING_EDGE ? EventType::rising : EventType::falling);
                }
            }
        } catch(std::exception& e) {
            LogERR("impulzy gpiochip: {}", e.what());
            return;
        }
    });
}

Vodomer::Vodomer(IMqttPublisher& mqtt)
    : Impulzy("0", 3, mqtt, "vodomer_total_litre", gpiod::line::edge::BOTH, 400)
{
    minDeltoToSTore = 10;
    threadName = "bytd-vodomer";
    svc_init();
}

Vodomer::~Vodomer()
{
}

namespace vodomer{
constexpr double LperH_2_LperMin(double lperH) { return lperH / 60.0;}
constexpr double imp_per_liter = 1;
constexpr double prietok_min = LperH_2_LperMin(31.25); // Zenner Q3=2.5
constexpr std::chrono::milliseconds intervalMax((unsigned)(((1/imp_per_liter) / prietok_min) * 60e3));
}

void Vodomer::event(EventType e)
{
    if( e != lastDebugEvent)
    {
        LogDBG("vodomer event {}", (int)e);
    }
    lastDebugEvent = e;

    if( e != EventType::timeout){
        lastEvent = e;
        return;
    }
    else{
        if(lastEvent != lastValidEvent){
            lastValidEvent = lastEvent;
            e = lastValidEvent;
        }
    }

    ImpulzyBase::event(e);

    if(e == EventType::rising) {
        LogINFO("vodomer imp rising");
        auto nt = Clock::now();
        lastPeriod = std::chrono::duration_cast<std::chrono::milliseconds>(nt - lastImp);
        lastImp = nt;
        if(lastPeriod > vodomer::intervalMax){
            prietok = vodomer::prietok_min;
        }
        else{
            prietok = (1 / vodomer::imp_per_liter) / (lastPeriod.count()/60e3);
        }
        mqtt.publish("rb/stat/prietok", std::to_string(prietok));
    } else if(e == EventType::falling) {
        LogINFO("vodomer imp falling");
    }
    else{
        if(prietok > 0 && std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - lastImp) > std::chrono::seconds(15)){
            prietok = 0;
            mqtt.publish("rb/stat/prietok", std::to_string(prietok));
        }
    }
}

float Vodomer::calc() const
{
    return orig + impCount / vodomer::imp_per_liter;
}
