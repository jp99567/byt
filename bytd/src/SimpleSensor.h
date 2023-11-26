#pragma once

#include "IIo.h"
#include "mqtt.h"

class SimpleSensor : public ISensorInput
{
public:
    SimpleSensor(std::string name, MqttClientSPtr mqtt);
    float value() const override;
    std::string name() const override;

private:
    void update(float v) override;
    const std::string topic;
    MqttClientSPtr mqtt;
    float val = std::numeric_limits<float>::quiet_NaN();
    event::Event<float> ChangedEvent;
};

