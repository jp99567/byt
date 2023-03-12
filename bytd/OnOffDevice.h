#pragma once

#include <memory>
#include "mqtt.h"

class IDigiOut;

class OnOffDevice
{
public:
    explicit OnOffDevice(std::unique_ptr<IDigiOut> out, std::string name, std::shared_ptr<MqttClient> mqtt);
    void toggle();
    void set(bool on=true);
    bool get() const;
    static constexpr auto mqttPathPrefix = "rb/ctrl/dev/";
private:
    std::unique_ptr<IDigiOut> out;
    const std::string name;
    const std::string mqttPath;
    std::shared_ptr<MqttClient> mqtt;
};

