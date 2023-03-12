#include "OnOffDevice.h"
#include "IIo.h"
#include "Log.h"

OnOffDevice::OnOffDevice(std::unique_ptr<IDigiOut> out, std::string name, std::shared_ptr<MqttClient> mqtt)
    :out(std::move(out))
    ,name(name)
    ,mqttPath(std::string(mqttPathPrefix).append(name))
    ,mqtt(mqtt){}

void OnOffDevice::toggle()
{
    auto newVal = ! (bool)*out;
    (*out)(newVal);
    mqtt->publish(mqttPath, (int)newVal);
}

void OnOffDevice::set(bool on)
{
    LogDBG("OnOffDevice::set check {} {}", on, (bool)*out);
    if(on != (bool)*out){
        (*out)(on);
        LogDBG("OnOffDevice::set {}", on);
        mqtt->publish(mqttPath, (int)on);
    }
}

bool OnOffDevice::get() const
{
    return *out;
}
