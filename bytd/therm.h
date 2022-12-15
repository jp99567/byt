#pragma once

#include <memory>
#include "data.h"

namespace ow {
	class OwThermNet;
}

struct Sensor {
    explicit Sensor(const ow::RomCode& rc, std::string name)
    :romcode(rc)
    ,name(name)
    ,curVal(std::numeric_limits<float>::quiet_NaN())
    ,lastValid(curVal){}

    //Sensor(const Sensor&) = delete;

    bool update(float v);

    ow::RomCode romcode;
    std::string name;
    float curVal;
    float lastValid;
};

class Pru;
class MqttClient;

class MeranieTeploty
{
public:
    explicit MeranieTeploty(std::shared_ptr<Pru> pru, std::vector<std::tuple<std::string, std::string>> reqsensors, MqttClient& mqtt);
	~MeranieTeploty();
	void meas();

private:
	std::unique_ptr<ow::OwThermNet> therm;
    std::vector<Sensor> sensors;
    MqttClient& mqtt;
};
