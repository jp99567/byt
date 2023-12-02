/*
 * Elektromer.h
 *
 *  Created on: Jun 23, 2019
 *      Author: j
 */
#pragma once

#include "bytd/src/IMqttPublisher.h"
#include <thread>
#include <chrono>
#include <string>

class MqttClient;

class Impulzy
{
public:
    explicit Impulzy(std::string chipname, unsigned line);
    virtual ~Impulzy();
protected:
    std::thread t;
    bool active = true;
    using Clock = std::chrono::steady_clock;
    Clock::time_point lastImp = Clock::time_point::min();
    enum class EventType { rising, falling, timeout };
    void svc_init();
    std::string threadName = "bytd-noname";
private:
    virtual void event(EventType){};
    std::string chipName;
    unsigned chipLine = 0;
};

class Elektromer : public Impulzy
{
public:
    explicit Elektromer(IMqttPublisher& mqtt);
    ~Elektromer() override;

	double getPowerCur() const;
	double getKWh() const;
private:
    void event(EventType) override;
	std::chrono::milliseconds lastPeriod = std::chrono::milliseconds::max();
	unsigned impCount = 0;
	double curPwr = std::numeric_limits<double>::quiet_NaN();
    IMqttPublisher& mqtt;
};

class Vodomer : public Impulzy
{
public:
    explicit Vodomer(MqttClient& mqtt);
    ~Vodomer() override;

private:
    void event(EventType) override;
    std::chrono::milliseconds lastPeriod = std::chrono::milliseconds::max();
    unsigned impCount = 0;
    MqttClient& mqtt;
};
