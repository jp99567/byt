#include "mqtt.h"
#include "Log.h"

MqttWrapper::MqttWrapper() noexcept
{
    mosqpp::lib_init();
}

MqttWrapper::~MqttWrapper()
{
    mosqpp::lib_cleanup();
}

MqttClient::MqttClient()
    :mosqpp::mosquittopp("bbb", false)
{
    connecting = true;
    auto rv = connect_async("localhost", 1883, 900);
    if( rv != MOSQ_ERR_SUCCESS){
        if( rv == MOSQ_ERR_ERRNO)
        {
            LogSYSERR("mosquitto_connect syserr");
        }
        else{
            LogERR("mosquitto_connect: {}", rv);
        }
    }
}

MqttClient::~MqttClient()
{
}

namespace {
bool handleIOreturnCode(int rv, const char* iosrc)
{
    if( rv != MOSQ_ERR_SUCCESS){
        if( rv == MOSQ_ERR_ERRNO)
        {
            LogSYSDIE("mosquitto loop io syserr");
        }
        else if(rv != MOSQ_ERR_CONN_LOST){
            LogDIE("mosquitto_loop_{} error {}", iosrc, rv);
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

void MqttClient::do_misc()
{
    auto rv = loop_misc();
    if( rv != MOSQ_ERR_SUCCESS){
        if( rv == MOSQ_ERR_NO_CONN){
            if(not connecting){
                connecting = true;
                LogERR("mosquitto_loop_misc: reconnect");
                reconnect_async();
                return;
            }
        }
        LogERR("mosquitto_loop_misc: {}", rv);
    }
}

void MqttClient::subscribe(/*ToDo*/)
{
    const char* const topics[] = {"ha/svetla", "ha/temp"};
    for(const auto& topic : topics){
        auto rv = mosqpp::mosquittopp::subscribe(nullptr, topic);
        if(rv != MOSQ_ERR_SUCCESS)
            LogERR("mosquitto_subscribe:{}", rv);
    }
}

void MqttClient::on_connect(int rc)
{
    if(rc == 0){
        LogINFO("on mqtt connect");
        ConnectedCbk(true);
    }
    else{
        LogERR("on mqtt connect: {}", rc);
    }
}

void MqttClient::on_disconnect(int rc)
{
    LogERR("on mqtt disconnect {}", rc);
    connecting = false;
    ConnectedCbk(false);
}

void MqttClient::on_message(const mosquitto_message *msg)
{
    std::string s((const char*)msg->payload, msg->payloadlen);
    LogDBG("mqtt msg: {} {} {}", msg->mid, msg->topic, s);
    OnMsgRecv(std::string(msg->topic), std::move(s));
}
