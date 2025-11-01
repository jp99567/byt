/*
 * Elektromer.h
 *
 *  Created on: Jun 23, 2019
 *      Author: j
 */
#pragma once

#include <thread>
#include "ImpulzyBase.h"

namespace gpiod::line {
enum class edge;
}

class Impulzy : public ImpulzyBase {
protected:
    explicit Impulzy(std::string chipname, unsigned line, IMqttPublisher& mqtt, const char* filename, const ::gpiod::line::edge line_req_type, int timeout_ms=1000);
    ~Impulzy() override;
    std::thread t;
    bool active = true;
    void svc_init();
    std::string threadName = "bytd-noname";

private:
    std::string chipName;
    unsigned chipLine = 0;
    const ::gpiod::line::edge line_req_type;
    const std::chrono::nanoseconds timeout;
};

class Elektromer : public Impulzy {
public:
    explicit Elektromer(IMqttPublisher& mqtt);
    ~Elektromer() override;

    double getPowerCur() const;
    double getKWh() const;

private:
    void event(EventType) override;
    std::chrono::milliseconds lastPeriod = std::chrono::milliseconds::max();
    double curPwr = std::numeric_limits<double>::quiet_NaN();
    float calc() const override;
};

class Vodomer : public Impulzy {
public:
    explicit Vodomer(IMqttPublisher& mqtt);
    ~Vodomer() override;

private:
    void event(EventType) override;
    std::chrono::milliseconds lastPeriod = std::chrono::milliseconds::max();
    EventType lastValidEvent = EventType::rising;
    EventType lastEvent = EventType::rising;
    EventType lastDebugEvent = EventType::rising;
    float calc() const override;
    float prietok = 0;
};
