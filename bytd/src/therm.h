#pragma once

#include <memory>
#include "data.h"
#include "IIo.h"

namespace ow {
	class OwThermNet;

struct Sensor : SensorInput
{
    explicit Sensor(const ow::RomCode& rc, std::string name)
    :SensorInput({name})
    ,romcode(rc){}

    Sensor(const Sensor&) = delete;
    bool update(float v);
    const ow::RomCode romcode;
};

using SensorList = std::vector<std::unique_ptr<ow::Sensor>>;
}

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
