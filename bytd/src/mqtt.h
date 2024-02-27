#pragma once

#include "IMqttPublisher.h"
#include <atomic>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <functional>
#include <map>
#include <mosquittopp.h>
#include <string>

class MqttWrapper {
public:
    MqttWrapper() noexcept;
    ~MqttWrapper();
};

class MqttClient : public mosqpp::mosquittopp, public IMqttPublisher {
    boost::asio::io_service& io_context;
    std::unique_ptr<boost::asio::ip::tcp::socket> mqtt_socket;
    constexpr static int reconnect_delay_countdown_init = 60;
    int reconnect_delay_countdown = reconnect_delay_countdown_init;
    std::atomic_bool mqtt_pending_write = false;
    bool connecting = false;
    std::map<std::string, std::string> ensure_publish;
    SensorDict registeredSensors;

public:
    explicit MqttClient(boost::asio::io_service& io_context);
    ~MqttClient() override;
    void do_misc();
    bool publish(const std::string& topic, const double value, bool retain = true) override;
    bool publish(const std::string& topic, const int value, bool retain = true) override;
    bool publish(const std::string& topic, const std::string& value, bool retain = true, int qos = 0) override;

    void registerSensor(std::string name, ISensorInput& sens) override;
    SensorDict& sensors() override;

    std::function<void(std::string&& topic, std::string&& msg)> OnMsgRecv;

private:
    void on_connect(int rc) override;
    void on_disconnect(int rc) override;
    void on_message(const struct mosquitto_message* message) override;
    bool do_read();
    bool do_write();
    void check_connect_attempt(int rv);
    void sched_read_mqtt_socket();
    void sched_write_mqtt_socket();
    void new_conn_init_wait();
};
