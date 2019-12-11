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
	auto nt = Clock::now();

	auto d = nt - lastImp;

	if( d > lastPeriod)
	{
		LogINFO("check1 {}s", std::chrono::duration_cast<std::chrono::seconds>(d).count());
		return calcPwr(std::chrono::duration_cast<std::chrono::milliseconds>(d));
	}

	return calcPwr(lastPeriod);
}

double Elektromer::getKWh() const
{
	return impCount / imp_per_kwh;
}

Elektromer::Elektromer()
{
	t = std::thread([this]{
		thread_util::set_thread_name("bytd-elektromer");

		gpiod::chip gpiochip("1");
		auto impin = gpiochip.get_line(17);

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
				if( impin.event_read().event_type == gpiod::line_event::RISING_EDGE )
				{
					++impCount;
					auto nt = Clock::now();
					lastPeriod = std::chrono::duration_cast<std::chrono::milliseconds>(nt - lastImp);
					lastImp = nt;
				}
			}
			auto pwr = getPowerCur();
			LogINFO("Elekromer: {}W {}s", pwr, lastPeriod.count()/1000.0);
			std::ofstream f("/run/cur_power_watts");
			f << pwr << '\n';
		}
	});
}

Elektromer::~Elektromer()
{
	active = false;
	t.join();
}
