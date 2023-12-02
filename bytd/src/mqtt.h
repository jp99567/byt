#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <atomic>
#include <functional>
#include <mosquittopp.h>
#include <string>
#include <map>

namespace mqtt {
 constexpr auto rbResponse = "rb/stat/response";
 constexpr auto rootTopic = "rb/ctrl/";
 constexpr auto devicesTopic = "rb/ctrl/dev/";
}

class MqttWrapper
{
public:
    MqttWrapper() noexcept;
    ~MqttWrapper();
};

class IMqttPublisher
{
public:
    virtual bool publish(const std::string& topic, const double value, bool retain = true) = 0;
    virtual bool publish(const std::string& topic, const int value, bool retain = true) = 0;
    virtual bool publish(const std::string& topic, const std::string& value, bool retain = true) = 0;
    virtual void publish_ensured(const std::string& topic, const std::string& value) = 0;
    virtual ~IMqttPublisher(){}
};

class MqttClient : public mosqpp::mosquittopp, public IMqttPublisher
{
    boost::asio::io_service& io_context;
    std::unique_ptr<boost::asio::ip::tcp::socket> mqtt_socket;
    constexpr static int reconnect_delay_countdown_init = 60;
    int reconnect_delay_countdown = reconnect_delay_countdown_init;
    std::atomic_bool mqtt_pending_write = false;
    bool connecting = false;
    std::map<std::string, std::string> ensure_publish;
public:
    explicit MqttClient(boost::asio::io_service& io_context);
    ~MqttClient() override;
    void do_misc();
    bool publish(const std::string& topic, const double value, bool retain = true) override;
    bool publish(const std::string& topic, const int value, bool retain = true) override;
    bool publish(const std::string& topic, const std::string& value, bool retain = true) override;
    void publish_ensured(const std::string& topic, const std::string& value) override;

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

using MqttClientSPtr = std::shared_ptr<IMqttPublisher>;
