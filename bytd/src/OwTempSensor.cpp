#include "OwTempSensor.h"
#include "bytd/src/Log.h"

void ow::Sensor::setValue(int16_t intval)
{
    float v = intval / factor;
    if(v==85){
        if(not (std::abs(value() - 85) < 1)){
            LogERR("ow {} unexpected 85", name());
            update(std::numeric_limits<float>::quiet_NaN());
            return;
        }
    }
    if(v > 125 || v < -55){
        LogERR("ow {} out of range:{}({})", name(), v, intval);
        update(std::numeric_limits<float>::quiet_NaN());
        return;
    }
    update(v);
}
