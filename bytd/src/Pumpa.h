#pragma once

#include <memory>

#include "IIo.h"
#include "IMqttPublisher.h"

/*** ToDo ******
 * tMAX = 720seconds
 * boiler Tmax = 50deg
 ****************/
class Pumpa {
    constexpr static int maxPlamenOff = 6;
    std::unique_ptr<IDigiOut> out;
    int plamenOffCount = -1;
    IMqttPublisherSPtr mqtt;

public:
    explicit Pumpa(std::unique_ptr<IDigiOut> digiout, IMqttPublisherSPtr mqtt);
    void start();
    void stop();
    void onPlamenOff();
    ~Pumpa();

private:
    void setPumpa(bool value);
};
