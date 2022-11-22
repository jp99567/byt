#pragma once

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

private:
    struct mosquitto* obj = nullptr;
};

