#pragma once

#include <memory>
#include "data.h"
#include "OwTempSensor.h"

class Pru;
class MqttClient;

class MeranieTeploty
{
public:
    explicit MeranieTeploty(std::shared_ptr<Pru> pru, ow::SensorList reqsensors, MqttClient& mqtt);
	~MeranieTeploty();
	void meas();

private:
	std::unique_ptr<ow::OwThermNet> therm;
    ow::SensorList sensors;
    MqttClient& mqtt;
};
