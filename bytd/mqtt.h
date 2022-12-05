#pragma once

#include <functional>
#include <mosquitto.h>
#include <string>

class MqttWrapper
{
public:
    MqttWrapper() noexcept;
    ~MqttWrapper();
};

struct mosquitto;

class MqttClient
{
public:
    MqttClient();
    ~MqttClient();
    int socket() const;
    bool do_read();
    bool do_write();
    void do_misc();
    bool is_write_ready() const;
    void subscribe(/*ToDo*/);

    std::function<void(bool)> ConnectedCbk;
    std::function<void(std::string &&topic, std::string &&msg)> OnMsgRecv;

private:
    struct mosquitto* obj = nullptr;
};

