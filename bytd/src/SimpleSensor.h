#pragma once

#include "IIo.h"
#include "mqtt.h"

class SimpleSensor : public ISensorInput
{
public:
    explicit SimpleSensor(std::string name, MqttClientSPtr mqtt);
    float value() const override;
    std::string name() const override;
    event::Event<float>& Changed() override { return ChangedEvent; }
    void setValue(float v);
protected:
    void update(float v) override;
private:
    const std::string topic;
    MqttClientSPtr mqtt;
    float val = std::numeric_limits<float>::quiet_NaN();
    event::Event<float> ChangedEvent;
};
