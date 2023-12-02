#include "Pumpa.h"

#include "Log.h"

Pumpa::Pumpa(std::unique_ptr<IDigiOut> digiout, IMqttPublisherSPtr mqtt)
    :out(std::move(digiout))
    ,mqtt(mqtt)
{
    (*out)(false);
}

void Pumpa::start()
{
    LogINFO("Pumpa start {}", plamenOffCount);
    setPumpa(true);
    plamenOffCount = 0;
}

void Pumpa::stop()
{
    LogINFO("Pumpa stop {}", plamenOffCount);
    setPumpa(false);
    plamenOffCount = -1;
}

void Pumpa::onPlamenOff()
{
    if(plamenOffCount >= 0){
        LogINFO("Pumpa onPlamenOff {}", plamenOffCount);
        if(++plamenOffCount > maxPlamenOff){
            stop();
            LogINFO("pumpa automatic Off");
        }
    }
}

Pumpa::~Pumpa()
{
    (*out)(false);
    LogDBG("~Pumpa");
}

void Pumpa::setPumpa(bool value)
{
    if( value != *out)
    {
        (*out)(value);
        mqtt->publish("rb/stat/pumpa", (int)value);
    }
}
