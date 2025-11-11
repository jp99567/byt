#include "mqtt.h"
#include "IIo.h"
#include "Log.h"
#include "hwif.h"

MqttWrapper::MqttWrapper() noexcept
{
    mosqpp::lib_init();
}

MqttWrapper::~MqttWrapper()
{
    mosqpp::lib_cleanup();
}

MqttClient::MqttClient(boost::asio::io_service& io_context)
    : mosqpp::mosquittopp("bbb", false)
    , io_context(io_context)
{
    threaded_set(true);
    auto broker_hostname = hwif::mqtt_broker_hostname();
    LogINFO("mqtt connect to {}", broker_hostname);
    auto rv = connect_async(broker_hostname.c_str(), 1883, 900);
    check_connect_attempt(rv);
}

void MqttClient::new_conn_init_wait()
{
    auto sfd = socket();
    if(sfd >= 0) {
        mqtt_socket = std::make_unique<boost::asio::ip::tcp::socket>(io_context, boost::asio::ip::tcp::v4(), sfd);
        sched_read_mqtt_socket();
        sched_write_mqtt_socket();
    }
}

MqttClient::~MqttClient()
{
}

namespace {
bool handleIOreturnCode(int rv, const char* iosrc)
{
    if(rv != MOSQ_ERR_SUCCESS) {
        if(rv == MOSQ_ERR_ERRNO) {
            LogSYSERR("mosquitto loop io");
        } else {
            LogERR("mosquitto_loop_{} error ({}) {}", iosrc, rv, mosqpp::strerror(rv));
        }
        return false;
    }
    return true;
}
}

bool MqttClient::do_read()
{
    auto rv = loop_read();
    return handleIOreturnCode(rv, "read");
}

bool MqttClient::do_write()
{
    auto rv = loop_write();
    return handleIOreturnCode(rv, "write");
}

void MqttClient::check_connect_attempt(int rv)
{
    if(rv != MOSQ_ERR_SUCCESS) {
        if(rv == MOSQ_ERR_ERRNO) {
            LogSYSERR("mosquitto_connect syserr");
        } else {
            LogERR("mosquitto_connect: ({}) {}", rv, mosqpp::strerror(rv));
        }
    } else {
        new_conn_init_wait();
    }
}

void MqttClient::do_misc()
{
    auto rv = loop_misc();
    if(rv != MOSQ_ERR_SUCCESS) {
        if(rv == MOSQ_ERR_NO_CONN) {
            if(--reconnect_delay_countdown < 0) {
                LogERR("mosquitto_loop_misc: reconnect");
                reconnect_delay_countdown = reconnect_delay_countdown_init;
                check_connect_attempt(reconnect_async());
                return;
            }
        }
        LogERR("mosquitto_loop_misc: ({}) {}", rv, mosqpp::strerror(rv));
    } else {
        sched_write_mqtt_socket();
    }
}

bool MqttClient::publish(const std::string& topic, const std::string& value, bool retain, int qos)
{
    // LogDBG("MqttClient::publish {} {} {}", topic, value, retain);
    auto rv = mosqpp::mosquittopp::publish(nullptr, topic.c_str(), value.length(), value.c_str(), qos, retain);
    if(rv != MOSQ_ERR_SUCCESS) {
        if(rv != MOSQ_ERR_NO_CONN)
            LogERR("mosqpp::mosquittopp::publish {}", mosqpp::strerror(rv));
        return false;
    }
    sched_write_mqtt_socket();
    return true;
}

void MqttClient::registerSensor(std::string name, ISensorInput& sens)
{
    registeredSensors.emplace(name, sens);
}

SensorDict& MqttClient::sensors()
{
    return registeredSensors;
}

bool MqttClient::publish(const std::string& topic, const double value, bool retain)
{
    return publish(topic, std::to_string(value), retain);
}

bool MqttClient::publish(const std::string& topic, const int value, bool retain)
{
    return publish(topic, std::to_string(value), retain);
}

void MqttClient::on_connect(int rc)
{
    if(rc == 0) {
        LogINFO("on mqtt connect");
        auto rv = mosqpp::mosquittopp::subscribe(nullptr, std::string(mqtt::rootTopic).append("#").c_str());
        if(rv != MOSQ_ERR_SUCCESS)
            LogERR("mosquitto_subscribe: ({}) {}", rv, mosqpp::strerror(rv));
    } else {
        LogERR("on mqtt connect: ({}) {}", rc, mosqpp::strerror(rc));
    }
}

void MqttClient::on_disconnect(int rc)
{
    LogERR("on mqtt disconnect ({}) {}", rc, mosqpp::strerror(rc));
    mqtt_socket = nullptr;
}

void MqttClient::on_message(const mosquitto_message* msg)
{
    std::string s((const char*)msg->payload, msg->payloadlen);
    OnMsgRecv(std::string(msg->topic), std::move(s));
}

void MqttClient::sched_read_mqtt_socket()
{
    if(not mqtt_socket) {
        LogDIE("unexpected MqttClient::sched_read_mqtt_socket nullptr");
    }
    mqtt_socket->async_wait(boost::asio::socket_base::wait_read, [this](const boost::system::error_code& error) {
        if(!error) {
            if(do_read()) {
                sched_read_mqtt_socket();
                sched_write_mqtt_socket();
            }
        } else {
            LogERR("async wait_read cbk:({}){}", error.value(), error.message());
        }
    });
}

void MqttClient::sched_write_mqtt_socket()
{
    if(want_write()) {
        auto isAlreadyPending = mqtt_pending_write.exchange(true);
        if(isAlreadyPending)
            return;
        if(not mqtt_socket) {
            LogDIE("unexpected MqttClient::sched_write_mqtt_socket nullptr");
        }
        mqtt_socket->async_wait(boost::asio::socket_base::wait_write, [this](const boost::system::error_code& error) {
            mqtt_pending_write = false;
            if(!error) {
                if(do_write())
                    sched_write_mqtt_socket();
            } else {
                LogERR("async wait_write cbk:({}){}", error.value(), error.message());
            }
        });
    }
}
