#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <atomic>
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
    boost::asio::io_service& io_context;
    std::unique_ptr<boost::asio::ip::tcp::socket> mqtt_socket;
    constexpr static int reconnect_delay_countdown_init = 60;
    int reconnect_delay_countdown = reconnect_delay_countdown_init;
    std::atomic_bool mqtt_pending_write = false;
    bool connecting = false;
public:
    MqttClient(boost::asio::io_service& io_context);
    ~MqttClient();
    void do_misc();
    void publish(const std::string& topic, const std::string& value, bool retain = true);

    std::function<void(std::string &&topic, std::string &&msg)> OnMsgRecv;

private:
    void on_connect(int rc) override;
    void on_disconnect(int rc) override;
    void on_message(const struct mosquitto_message * message) override;
    bool do_read();
    bool do_write();
    void check_connect_attempt(int rv);
    void sched_read_mqtt_socket();
    void sched_write_mqtt_socket();
    void new_conn_init_wait();
};

