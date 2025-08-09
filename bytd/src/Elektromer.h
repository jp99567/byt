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

class Impulzy {
    std::string chipName;
    unsigned chipLine = 0;
public:
    void store();
    void setNewValue(float newVal);
    virtual ~Impulzy();
protected:
    explicit Impulzy(std::string chipname, unsigned line, IMqttPublisher& mqtt, const char* filename, int line_req_type);
    void store(float val);
    std::thread t;
    bool active = true;
    using Clock = std::chrono::steady_clock;
    Clock::time_point lastImp = Clock::time_point::min();
    enum class EventType { rising,
        falling,
        timeout };
    void svc_init();
    std::string threadName = "bytd-noname";
    unsigned impCount = 0;
    virtual void event(EventType);
    IMqttPublisher& mqtt;
    float orig = 0;
    float minDeltoToSTore = 1;

private:
    std::filesystem::path persistFile;
    std::string mqtt_topic_total;
    float lastStored = std::numeric_limits<float>::quiet_NaN();
    Clock::time_point lastStoredTime;
    const int line_req_type;
    void checkStore();
    virtual float calc() const = 0;
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
    Clock::time_point lastChangeDebounced = Clock::time_point::min();
    float calc() const override;
    float prietok = 0;
};
