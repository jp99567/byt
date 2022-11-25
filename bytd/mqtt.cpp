#include "mqtt.h"

#include<mosquitto.h>
#include "Log.h"

MqttWrapper::MqttWrapper() noexcept
{
    mosquitto_lib_init();
}

MqttWrapper::~MqttWrapper()
{
    mosquitto_lib_cleanup();
}

MqttClient::MqttClient()
{
    obj = mosquitto_new("bbb", false, (void*)this);
    if(!obj)
    {
        LogDIE("MqttClient new");
    }

    mosquitto_connect_callback_set(obj, [](struct mosquitto* client, void* self, int rc){
        if(rc == 0){
            LogINFO("on mqtt connect");
            auto theInstance = static_cast<MqttClient*>(self);
            if(theInstance->ConnectedCbk)
                theInstance->ConnectedCbk(true);
        }
        else{
            LogERR("on mqtt connect: {}", rc);
        }
    });

    mosquitto_disconnect_callback_set(obj, [](struct mosquitto* client, void* self, int rc){
        LogERR("on mqtt disconnect {}", rc);
        auto theInstance = static_cast<MqttClient*>(self);
        if(theInstance->ConnectedCbk)
            theInstance->ConnectedCbk(false);
    });

    mosquitto_message_callback_set(obj, [](struct mosquitto*, void* userData, const struct mosquitto_message* msg){
        std::string s((const char*)msg->payload, msg->payloadlen);
        LogDBG("mqtt msg: {} {} {}", msg->mid, msg->topic, s);
        auto self = static_cast<MqttClient*>(userData);
        if(self->OnMsgRecv)
            self->OnMsgRecv(std::string(msg->topic), std::move(s));
    });

    auto rv = mosquitto_connect_async(obj, "localhost", 1883, 900);
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

int MqttClient::socket() const
{
    return mosquitto_socket(obj);
}

void MqttClient::do_read()
{
    auto rv = mosquitto_loop_read(obj, 1);
    if( rv != MOSQ_ERR_SUCCESS){
        if( rv == MOSQ_ERR_ERRNO)
        {
            LogSYSERR("mosquitto_loop_read syserr");
        }
        else{
            LogERR("mosquitto_loop_read: {}", rv);
        }
    }
}

void MqttClient::do_write()
{
    auto rv = mosquitto_loop_write(obj, 1);
    if( rv != MOSQ_ERR_SUCCESS){
        if( rv == MOSQ_ERR_ERRNO)
        {
            LogSYSERR("mosquitto_loop_write syserr");
        }
        else{
            LogERR("mosquitto_loop_write: {}", rv);
        }
    }
}

void MqttClient::do_misc()
{
    auto rv = mosquitto_loop_misc(obj);
    if( rv != MOSQ_ERR_SUCCESS){
        LogERR("mosquitto_loop_misc: {}", rv);
    }
}

bool MqttClient::is_write_ready() const
{
    return mosquitto_want_write(obj);
}

void MqttClient::subscribe(/*ToDo*/)
{
    const char* const topics[] = {"ha/svetla", "ha/temp"};
    auto rv = mosquitto_subscribe_multiple(obj, nullptr, 2, (char*const*)topics, 0, 0, nullptr);
    if(rv != MOSQ_ERR_SUCCESS)
        LogERR("mosquitto_subscribe_multiple:{}", rv);
}
