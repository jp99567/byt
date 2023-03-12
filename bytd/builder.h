#pragma once
#include <yaml-cpp/yaml.h>
#include <map>
#include "canbus.h"
#include "candata.h"
#include "OnOffDevice.h"
#include "mqtt.h"
#include "therm.h"

class Builder
{
    const YAML::Node config;
    std::shared_ptr<MqttClient> mqtt;
    std::map<std::string, std::unique_ptr<IDigiOut>> digiOutputs;
    std::vector<std::reference_wrapper<DigInput>> digInputs;
    std::vector<std::reference_wrapper<SensorInput>> sensors;
public:
    Builder(std::shared_ptr<MqttClient> mqtt);
    [[nodiscard]] std::unique_ptr<can::InputControl> buildCan(CanBus& canbus);
    [[nodiscard]] ow::SensorList buildBBoW();
    [[nodiscard]] std::map<const std::string, OnOffDevice> buildOnOffDevices();
};
