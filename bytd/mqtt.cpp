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
        LogINFO("on mqtt connect {}", rc);
        int mid=0;
        const char* const topics[] = {"ha/svetla", "ha/temp"};
        auto rv = mosquitto_subscribe_multiple(client, &mid, 2, (char*const*)topics, 0, 0, nullptr);
        LogINFO("mosquitto_subscribe_multiple mid:{} rv:{}", mid, rv);
    });

    mosquitto_disconnect_callback_set(obj, [](struct mosquitto* client, void* self, int rc){
        LogINFO("on mqtt disconnect {}", rc);
    });

    mosquitto_message_callback_set(obj, [](struct mosquitto*, void*, const struct mosquitto_message* msg){
        std::string s((const char*)msg->payload, msg->payloadlen);
        LogINFO("mqtt msg: {} {} {}", msg->mid, msg->topic, s);
    });

    auto rv = mosquitto_connect_async(obj, "localhost", 1883, 900);
    if( rv == MOSQ_ERR_ERRNO)
    {
        LogSYSERR("mosquitto_connect syserr");
    }
    else{
        LogINFO("mosquitto_connect: {}", rv);
    }

    auto fd = mosquitto_socket(obj);
    LogINFO("mqtt socket:{}", fd);
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
    if(rv == MOSQ_ERR_ERRNO){
        LogSYSERR("MqttClient::do_read");
    }
    else{
        LogINFO("MqttClient::do_read: {}", rv);
    }
}

void MqttClient::do_write()
{
    auto rv = mosquitto_loop_write(obj, 1);
    if(rv == MOSQ_ERR_ERRNO){
        LogSYSERR("MqttClient::do_write");
    }
    else{
        LogINFO("MqttClient::do_write: {}", rv);
    }
}

void MqttClient::do_misc()
{
    mosquitto_loop_misc(obj);
}
