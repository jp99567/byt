#pragma once

#include <memory>

#include "mqtt.h"
#include "IIo.h"

class Pumpa
{
    constexpr static int maxPlamenOff = 6;
    std::unique_ptr<IDigiOut> out;
    int plamenOffCount = -1;
    std::shared_ptr<MqttClient> mqtt;

public:
    explicit Pumpa(std::unique_ptr<IDigiOut> digiout, std::shared_ptr<MqttClient> mqtt);
    void start();
    void stop();
    void onPlamenOff();
    ~Pumpa();

private:
    void setPumpa(bool value);
};
