#include "OwTempSensor.h"
#include "Log.h"
#include "avr/fw/OwResponseCode_generated.h"

void ow::Sensor::setValue(int16_t intval)
{
    if(intval == ow::cInvalidValue) {
        update(std::numeric_limits<float>::quiet_NaN());
        return;
    }

    float v = intval / factor;
    if(v == 85) {
        if(not(std::abs(value() - 85) < 1)) {
            LogERR("ow {} unexpected 85", name());
            update(std::numeric_limits<float>::quiet_NaN());
            return;
        }
    }
    if(v > 125 || v < -55) {
        LogERR("ow {} out of range:{}({})", name(), v, intval);
        update(std::numeric_limits<float>::quiet_NaN());
        return;
    }
    update(v);
}
