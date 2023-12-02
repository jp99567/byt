#pragma once

#include "SimpleSensor.h"
#include "bytd/src/data.h"

namespace ow {
class OwThermNet;

struct Sensor : SimpleSensor
{
    explicit Sensor(std::string name, IMqttPublisherSPtr mqtt, float factor = 16.0)
        :SimpleSensor(name, mqtt)
        ,factor(factor){}

    Sensor(const Sensor&) = delete;
    void setValue(int16_t intval);
    const float factor;
};

using SensorList = std::vector<std::pair<ow::RomCode,std::unique_ptr<ow::Sensor>>>;
}
