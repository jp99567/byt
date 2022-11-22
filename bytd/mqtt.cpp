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
    });

    mosquitto_disconnect_callback_set(obj, [](struct mosquitto* client, void* self, int rc){
        LogINFO("on mqtt disconnect {}", rc);
    });

    auto rv = mosquitto_connect(obj, "localhost", 1883, 900);
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
