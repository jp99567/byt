#pragma once
#include <boost/asio/io_service.hpp>
#include <map>
#include <yaml-cpp/yaml.h>

#include "Kurenie.h"
#include "OnOffDevice.h"
#include "Reku.h"
#include "Ventil4w.h"
#include "Vypinace.h"
#include "bytd/src/Pumpa.h"
#include "canbus.h"
#include "candata.h"
#include "slowswi2cpwm.h"

using OnOffDeviceList = std::map<const std::string, std::unique_ptr<OnOffDevice>>;
using VypinaceDuoList = std::vector<std::unique_ptr<VypinacDuo>>;
using VypinaceSingleList = std::vector<std::unique_ptr<VypinacSingle>>;

namespace gpiod {
class chip;
}
class OpenTherm;
class Impulzy;

struct AppComponents {
    VypinaceDuoList vypinaceDuo;
    VypinaceSingleList vypinaceSingle;
    OnOffDeviceList devicesOnOff;
    std::unique_ptr<MonoStableDev> brana;
    std::unique_ptr<MonoStableDev> dverePavlac;
    std::unique_ptr<Reku> reku;
    std::unique_ptr<gpiod::chip> gpiochip3;
    std::unique_ptr<Pumpa> pumpa;
    std::unique_ptr<kurenie::Kurenie> kurenie;
    std::unique_ptr<Ventil4w> ventil;
    std::unique_ptr<Impulzy> elektromer;
    std::unique_ptr<Impulzy> vodomer;
};

class Builder {
    const YAML::Node config;
    IMqttPublisherSPtr mqtt;
    std::map<std::string, std::unique_ptr<IDigiOut>> digiOutputs;
    std::vector<std::reference_wrapper<DigInput>> digInputs;
    std::vector<std::reference_wrapper<ISensorInput>> sensors;

public:
    Builder(IMqttPublisherSPtr mqtt);
    void buildMisc(slowswi2cpwm& ioexpander, OpenTherm& ot);
    [[nodiscard]] std::unique_ptr<can::InputControl> buildCan(CanBus& canbus);
    [[nodiscard]] ow::SensorList buildBBoW();
    void vypinace(boost::asio::io_service& io_context);
    AppComponents getComponents();

private:
    // void buildDevice(std::string name, std::string outputName, std::string inputName = std::string());
    OnOffDevice& buildDevice(std::string name, std::string outputName, event::Event<>& controlEvent, bool outInverted = false);
    OnOffDevice& buildDeviceInvertedOut(std::string name, std::string outputName, event::Event<>& controlEvent)
    {
        return buildDevice(std::move(name), std::move(outputName), controlEvent, true);
    }
    AppComponents components;
    ISensorInput& findSensor(std::string name);
};
