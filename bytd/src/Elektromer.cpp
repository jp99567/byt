/*
 * Elektromer.cpp
 *
 *  Created on: Jun 23, 2019
 *      Author: j
 */

#include "Elektromer.h"
#include <gpiod.hpp>
#include "Log.h"
#include <fstream>
#include "thread_util.h"
#include "mqtt.h"

namespace
{

constexpr double imp_per_kwh = 1000;

constexpr double calcPwr(std::chrono::milliseconds ms)
{
	auto hour = ms.count() / 3600e3;
	auto kw = 1 / ( hour * imp_per_kwh );
	return kw * 1e3;
}

}

double Elektromer::getPowerCur() const
{
	if(lastPeriod == std::chrono::milliseconds::max())
		return 0.0;

	auto nt = Clock::now();

	auto d = nt - lastImp;

	if( d > lastPeriod)
	{
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
    if(e == EventType::rising){
        ++impCount;
        auto nt = Clock::now();
        lastPeriod = std::chrono::duration_cast<std::chrono::milliseconds>(nt - lastImp);
        lastImp = nt;
    }

    auto pwr = getPowerCur();
    if( pwr != curPwr){
        curPwr = pwr;
        mqtt.publish("rb/stat/power", std::to_string(pwr));
    }
}

Elektromer::Elektromer(MqttClient& mqtt)
    :Impulzy("1", 17)
    ,mqtt(mqtt)
{
    threadName = "bytd-elektromer";
    svc_init();
}

Elektromer::~Elektromer()
{
    LogDBG("~Elektromer");
}

Impulzy::Impulzy(std::string chipname, unsigned int line)
    :chipName(chipname)
    ,chipLine(line)
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
    t = std::thread([this]{
        thread_util::set_thread_name(threadName.c_str());

        gpiod::chip gpiochip(chipName);
        auto impin = gpiochip.get_line(chipLine);

        if( impin.direction() != gpiod::line::DIRECTION_INPUT )
        {
            LogERR("Impulz direction not input");
            active = false;
        }
        impin.request({"bytd", gpiod::line_request::EVENT_RISING_EDGE});

        while(active){
            constexpr unsigned long long toutns = 1e9;
            if(impin.event_wait(std::chrono::nanoseconds(toutns)))
            {
                event(impin.event_read().event_type == gpiod::line_event::RISING_EDGE ? EventType::rising : EventType::falling);
            }
            else{
                event(EventType::timeout);
            }
        }
    });
}

Vodomer::Vodomer(MqttClient &mqtt)
    :Impulzy("0", 3)
    ,mqtt(mqtt)
{

}

Vodomer::~Vodomer()
{

}

constexpr double imp_per_liter = 1;

void Vodomer::event(EventType e)
{
    if(e == EventType::rising){
        ++impCount;
        auto nt = Clock::now();
        lastPeriod = std::chrono::duration_cast<std::chrono::milliseconds>(nt - lastImp);
        lastImp = nt;
    }
    else if(e == EventType::falling){
    }


    //    mqtt.publish("rb/stat/prietok", std::to_string(pwr));

}