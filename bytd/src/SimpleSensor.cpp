#include "SimpleSensor.h"
#include <cmath>
#include <filesystem>

constexpr auto topic_base = "rb/stat/sens/";

SimpleSensor::SimpleSensor(std::string name, IMqttPublisherSPtr mqtt)
    :topic(std::string(topic_base).append(name)), mqtt(mqtt)
{

}

float SimpleSensor::value() const
{
    return val;
}

std::string SimpleSensor::name() const
{

    std::filesystem::path t(topic);
    return (t.end()--)->native();
}

void SimpleSensor::setValue(float v)
{
    update(v);
}

void SimpleSensor::update(float v)
{
    if(std::isnan(v) && std::isnan(val)){
        return;
    }

    if(v != val){
        val = v;

        mqtt->publish(topic,  v);
        ChangedEvent.notify(val);
    }
}
