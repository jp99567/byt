#pragma once

#include "OwTempSensor.h"
#include "data.h"
#include <memory>

class Pru;
class MqttClient;

class MeranieTeploty {
public:
    explicit MeranieTeploty(std::shared_ptr<Pru> pru, ow::SensorList reqsensors, MqttClient& mqtt);
    ~MeranieTeploty();
    void meas();

private:
    std::unique_ptr<ow::OwThermNet> therm;
    ow::SensorList sensors;
    MqttClient& mqtt;
};
