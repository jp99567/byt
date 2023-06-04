#pragma once

#include "IIo.h"
#include "mqtt.h"

class MqttDigiOut : public IDigiOut
{
  std::shared_ptr<MqttClient> mqtt;
  const std::string path;
  bool valueCached = false;

public:
  MqttDigiOut(std::shared_ptr<MqttClient> mqtt, std::string path);
  operator bool() const override
  {
    return valueCached; // weak info. Does not reflect real value in mqtt broker
  }

  bool operator()(bool value) override;
  ~MqttDigiOut() override {}
};
