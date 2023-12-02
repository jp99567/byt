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
#include "slowswi2cpwm.h"
#include "Reku.h"

using OnOffDeviceList = std::map<const std::string, std::unique_ptr<OnOffDevice>>;
using VypinaceDuoList = std::vector<std::unique_ptr<VypinacDuo>>;
using VypinaceSingleList = std::vector<std::unique_ptr<VypinacSingle>>;

class Builder
{
    const YAML::Node config;
    MqttClientSPtr mqtt;
    std::map<std::string, std::unique_ptr<IDigiOut>> digiOutputs;
    std::vector<std::reference_wrapper<DigInput>> digInputs;
    std::vector<std::reference_wrapper<ISensorInput>> sensors;


public:
    struct AppComponents
    {
        VypinaceDuoList vypinaceDuo;
        VypinaceSingleList vypinaceSingle;
        OnOffDeviceList devicesOnOff;
        std::unique_ptr<MonoStableDev> brana;
        std::unique_ptr<MonoStableDev> dverePavlac;
        std::unique_ptr<Reku> reku;
    };

    Builder(MqttClientSPtr mqtt);
    void buildMisc(slowswi2cpwm& ioexpander);
    [[nodiscard]] std::unique_ptr<can::InputControl> buildCan(CanBus& canbus);
    [[nodiscard]] ow::SensorList buildBBoW();
    void vypinace(boost::asio::io_service& io_context);
    AppComponents getComponents();

private:
    //void buildDevice(std::string name, std::string outputName, std::string inputName = std::string());
    OnOffDevice& buildDevice(std::string name, std::string outputName, event::Event<>& controlEvent);
    AppComponents components;
};
