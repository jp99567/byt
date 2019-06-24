/*
 * Elektromer.h
 *
 *  Created on: Jun 23, 2019
 *      Author: j
 */
#pragma once

#include <thread>
#include <chrono>

class Elektromer
{
public:
	explicit Elektromer();
	~Elektromer();

	double getPowerCur() const;
	double getKWh() const;
private:
	std::thread t;
	bool active = true;
	using Clock = std::chrono::steady_clock;
	Clock::time_point lastImp = Clock::time_point::min();
	std::chrono::milliseconds lastPeriod = std::chrono::milliseconds::max();
	unsigned impCount = 0;
};

