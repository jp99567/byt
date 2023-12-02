#include "MqttDigiOut.h"

MqttDigiOut::MqttDigiOut(IMqttPublisherSPtr mqtt, std::string path)
    :mqtt(mqtt)
    ,path(path)
{
}

bool MqttDigiOut::operator()(bool value)
{
    valueCached = value;
    mqtt->publish(path, value);
    return valueCached;
}
