#pragma once

#include <string>
#include <memory>

namespace mqtt {
constexpr auto rbResponse = "rb/stat/response";
constexpr auto rootTopic = "rb/ctrl/";
constexpr auto devicesTopic = "rb/ctrl/dev/";
}

class IMqttPublisher
{
public:
    virtual bool publish(const std::string& topic, const double value, bool retain = true) = 0;
    virtual bool publish(const std::string& topic, const int value, bool retain = true) = 0;
    virtual bool publish(const std::string& topic, const std::string& value, bool retain = true) = 0;
    virtual void publish_ensured(const std::string& topic, const std::string& value) = 0;
    virtual ~IMqttPublisher(){}
};

using IMqttPublisherSPtr = std::shared_ptr<IMqttPublisher>;
