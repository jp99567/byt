#pragma once

#include<filesystem>

class IMqttPublisher;

class ImpulzyBase
{
public:
    void setNewValue(float newVal);
    void store();
    virtual ~ImpulzyBase() { }
protected:
    explicit ImpulzyBase(IMqttPublisher& mqtt, const char* filename);
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
