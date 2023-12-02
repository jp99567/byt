#pragma once

#include "IIo.h"
#include "IMqttPublisher.h"

class MqttDigiOut : public IDigiOut
{
    IMqttPublisherSPtr mqtt;
  const std::string path;
  bool valueCached = false;

public:
  MqttDigiOut(IMqttPublisherSPtr mqtt, std::string path);
  operator bool() const override
  {
    return valueCached; // weak info. Does not reflect real value in mqtt broker
  }

  bool operator()(bool value) override;
  ~MqttDigiOut() override {}
};
