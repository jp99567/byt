#pragma once

#include <string>
#include <memory>
#include <map>
#include <string>

namespace mqtt {
constexpr auto rbResponse = "rb/stat/response";
constexpr auto rootTopic = "rb/ctrl/";
constexpr auto devicesTopic = "rb/ctrl/dev/";
constexpr auto tevOverrideTopic = "rb/ctrl/override/tev/";
constexpr auto kurenieSetpointTopic = "rb/ctrl/kurenie/sp/";
}

class ISensorInput;
using SensorDict = std::map<std::string, std::reference_wrapper<ISensorInput>>;

class IMqttPublisher
{
public:

    virtual bool publish(const std::string& topic, const double value, bool retain = true) = 0;
    virtual bool publish(const std::string& topic, const int value, bool retain = true) = 0;
    virtual bool publish(const std::string& topic, const std::string& value, bool retain = true) = 0;
    virtual void publish_ensured(const std::string& topic, const std::string& value) = 0;

    virtual void registerSensor(std::string name, ISensorInput& sens) = 0;
    virtual SensorDict& sensors() = 0;
    virtual ~IMqttPublisher(){}
};

using IMqttPublisherSPtr = std::shared_ptr<IMqttPublisher>;
