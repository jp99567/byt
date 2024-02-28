#pragma once

#include "SimpleSensor.h"
#include "avr/fw/sensirion_common.h"

namespace Sensorion {

using Conversion = float(double);

constexpr float convSHT11_T(double v_adc)
{
    constexpr float d1 = -39.6, d2 = 0.01;
    return d1 + d2 * v_adc;
}

constexpr float convSHT11_RH(double v_adc)
{
    constexpr float c1 = -2.0468, c2 = 0.0367, c3 = -1.5955E-6;
    return c1 + c2 * v_adc + c3 * v_adc * v_adc;
}

constexpr float convSCD41_T(double temp_adc)
{
    return -45 + 175 * (temp_adc / ((1<<16) - 1));
}

constexpr float convSCD41_RH(double rh_adc)
{
    return 100 * (rh_adc / ((1<<16) - 1));
}

constexpr float convSCD41_CO2(double v_adc)
{
    return v_adc;
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
