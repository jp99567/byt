#pragma once

#include <functional>
#include <mosquittopp.h>
#include <string>

class MqttWrapper
{
public:
    MqttWrapper() noexcept;
    ~MqttWrapper();
};

class MqttClient : public mosqpp::mosquittopp
{
public:
    MqttClient();
    ~MqttClient();
    bool do_read();
    bool do_write();
    void do_misc();
    void subscribe(/*ToDo*/);

    std::function<void(bool)> ConnectedCbk;
    std::function<void(std::string &&topic, std::string &&msg)> OnMsgRecv;

private:
    bool connecting = false;
    void on_connect(int rc) override;
    void on_disconnect(int rc) override;
    void on_message(const struct mosquitto_message * message) override;
};

