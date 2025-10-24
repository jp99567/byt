/*
 * Elektromer.h
 *
 *  Created on: Jun 23, 2019
 *      Author: j
 */
#pragma once

#include "bytd/src/IMqttPublisher.h"
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

class MqttClient;

class ImpulzyBase
{
public:
    void setNewValue(float newVal);
    void store();
protected:
    explicit ImpulzyBase(IMqttPublisher& mqtt, const char* filename);
    virtual ~ImpulzyBase() { }
    IMqttPublisher& mqtt;
    using Clock = std::chrono::steady_clock;
    Clock::time_point lastImp = Clock::time_point::min();
    enum class EventType { rising = 1,
        falling = 0,
        timeout = 2};
    float orig = 0;
    float minDeltoToSTore = 1;
    unsigned impCount = 0;
    virtual void event(EventType);
    void store(float val);
private:
    std::filesystem::path persistFile;
    std::string mqtt_topic_total;
    float lastStored = std::numeric_limits<float>::quiet_NaN();
    Clock::time_point lastStoredTime;
    void checkStore();
    virtual float calc() const = 0;
};

class Impulzy : public ImpulzyBase {
public:
    ~Impulzy() override;
protected:
    explicit Impulzy(std::string chipname, unsigned line, IMqttPublisher& mqtt, const char* filename, int line_req_type, int timeout_ms=1000);
    std::thread t;
    bool active = true;
    void svc_init();
    std::string threadName = "bytd-noname";

private:
    std::string chipName;
    unsigned chipLine = 0;
    const int line_req_type;
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
