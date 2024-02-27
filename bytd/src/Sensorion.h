#pragma once

#include "SimpleSensor.h"
#include "avr/fw/sensirion_common.h"

namespace Sensorion {

using Conversion = float(uint16_t);

constexpr float convToDo(uint16_t v_u16)
{
    return (float)v_u16;
}

template <Conversion covFn>
struct Sensor : SimpleSensor {
    explicit Sensor(std::string name, IMqttPublisherSPtr mqtt)
        : SimpleSensor(name, mqtt)
    {
    }

    Sensor(const Sensor&) = delete;
    void setValue(uint16_t intval)
    {
        if(intval == cInvalidValue) {
            update(std::numeric_limits<float>::quiet_NaN());
            return;
        }

        update(covFn(intval));
    }
};

} // namespace Sensorion
