#include "ImpulzyBase.h"
#include "Log.h"
#include <fstream>
#include "IMqttPublisher.h"

ImpulzyBase::ImpulzyBase(IMqttPublisher &mqtt, const char *filename)
    : mqtt(mqtt)
    , persistFile(filename)
{
    try {
        std::ifstream f(persistFile);
        f >> lastStored;
    } catch(const std::exception& e) {
        LogERR("implzy last value not found ({})", persistFile.string());
    }
    if(not std::isnan(lastStored))
        orig = lastStored;
    LogINFO("Impulzy {} loaded {}", persistFile.native(), lastStored);
}

void ImpulzyBase::store()
{
    auto v = calc();
    store(v);
}

void ImpulzyBase::setNewValue(float newVal)
{
    impCount = 0;
    orig = newVal;
    store(newVal);
}

void ImpulzyBase::event(EventType e)
{
    if(e == EventType::rising) {
        ++impCount;
        checkStore();
    }
}

void ImpulzyBase::checkStore()
{
    auto v = calc();
    mqtt.publish(mqtt_topic_total, std::to_string(v));
    if(lastStoredTime + std::chrono::hours(1) < Clock::now()
        || std::abs(v - lastStored) > minDeltoToSTore) {
        store(v);
    }
}

void ImpulzyBase::store(float val)
{
    if(val != lastStored) {
        lastStored = val;
        std::ofstream f(persistFile);
        f << lastStored << std::endl;
        LogINFO("stored value {} to {}", val, persistFile.string());
    }
    lastStoredTime = Clock::now();
}

