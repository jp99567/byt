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
    int socket() const;
    void do_read();
    void do_write();
    void do_misc();

private:
    struct mosquitto* obj = nullptr;
};

