#include "OnOffDevice.h"
#include "IIo.h"
#include "Log.h"

OnOffDevice::OnOffDevice(std::unique_ptr<IDigiOut> out, std::string name, MqttClientSPtr mqtt)
    :DeviceBase(std::move(out), name, mqtt)
{}

void OnOffDevice::toggle()
{
    auto newVal = ! (bool)*out;
    (*out)(newVal);
    mqtt->publish(mqttPath, (int)newVal);
}

void OnOffDevice::set(bool on, bool by_mqtt)
{
    LogDBG("OnOffDevice::set check {} {}", on, (bool)*out);
    if(on != (bool)*out){
        (*out)(on);
        LogDBG("OnOffDevice::set {}", on);
        if(not by_mqtt)
            mqtt->publish(mqttPath, (int)on);
    }
}

bool OnOffDevice::get() const
{
    return *out;
}

MonoStableDev::MonoStableDev(std::unique_ptr<IDigiOut> out, std::string name, MqttClientSPtr mqtt, boost::asio::io_service& io_context, float timeout_sec)
    :DeviceBase(std::move(out), name, mqtt)
    ,timer(io_context)
    ,timeout(std::chrono::milliseconds((int)(timeout_sec*1e3)))
{}

void MonoStableDev::trigger()
{
    if((*out)){
        LogDBG("MonoStableDev already triggered");
        return;
    }

    timer.expires_at(std::chrono::steady_clock::now() + timeout);
    timer.async_wait([this](boost::system::error_code){
        (*out)(false);
        mqtt->publish(mqttPath, 0);
        LogDBG("MonoStableDev cleared");
    });
    (*out)(true);
    LogDBG("MonoStableDev triggered");
}

DeviceBase::DeviceBase(std::unique_ptr<IDigiOut> out, std::string name, MqttClientSPtr mqtt)
    :out(std::move(out))
    ,name(name)
    ,mqttPath(std::string(mqtt::devicesTopic).append(name))
    ,mqtt(mqtt){}

