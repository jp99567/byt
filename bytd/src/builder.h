#pragma once
#include <yaml-cpp/yaml.h>
#include <map>
#include <boost/asio/io_service.hpp>

#include "canbus.h"
#include "candata.h"
#include "OnOffDevice.h"
#include "mqtt.h"
#include "therm.h"
#include "Vypinace.h"

using OnOffDeviceList = std::map<const std::string, std::unique_ptr<OnOffDevice>>;
using VypinaceDuoWithLongPressList = std::vector<std::unique_ptr<VypinacDuoWithLongPress>>;

class Builder
{
    const YAML::Node config;
    std::shared_ptr<MqttClient> mqtt;
    std::map<std::string, std::unique_ptr<IDigiOut>> digiOutputs;
    std::vector<std::reference_wrapper<DigInput>> digInputs;
    std::vector<std::reference_wrapper<SensorInput>> sensors;


public:
    struct AppComponents
    {
        VypinaceDuoWithLongPressList vypinaceDuo;
        OnOffDeviceList devicesOnOff;
        std::unique_ptr<MonoStableDev> brana;
        std::unique_ptr<MonoStableDev> dverePavlac;
    };

    Builder(std::shared_ptr<MqttClient> mqtt);
    [[nodiscard]] std::unique_ptr<can::InputControl> buildCan(CanBus& canbus);
    [[nodiscard]] ow::SensorList buildBBoW();
    void vypinace(boost::asio::io_service& io_context);
    AppComponents getComponents();

private:
    //void buildDevice(std::string name, std::string outputName, std::string inputName = std::string());
    void buildDevice(std::string name, std::string outputName, event::Event<>& controlEvent);
    AppComponents components;
};
