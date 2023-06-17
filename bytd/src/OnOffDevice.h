#pragma once

#include <memory>
#include "mqtt.h"
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

class IDigiOut;

class DeviceBase
{
public:
    static constexpr auto mqttPathPrefix = "rb/ctrl/dev/";
protected:
    explicit DeviceBase(std::unique_ptr<IDigiOut> out, std::string name, std::shared_ptr<MqttClient> mqtt);
    std::unique_ptr<IDigiOut> out;
    const std::string name;
    const std::string mqttPath;
    std::shared_ptr<MqttClient> mqtt;
};

class OnOffDevice : public DeviceBase
{
public:
    explicit OnOffDevice(std::unique_ptr<IDigiOut> out, std::string name, std::shared_ptr<MqttClient> mqtt);
    void toggle();
    void set(bool on, bool by_mqtt = false);
    bool get() const;
};

class MonoStableDev : public DeviceBase
{
public:
    explicit MonoStableDev(std::unique_ptr<IDigiOut> out, std::string name, std::shared_ptr<MqttClient> mqtt, boost::asio::io_service& io_context, float timeout_sec);
    void trigger();
private:
    boost::asio::steady_timer timer;
    const std::chrono::milliseconds timeout;
};
