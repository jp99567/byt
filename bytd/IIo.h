#pragma once

#include "Event.h"

class IDigiOut
{
public:
    virtual operator bool() const = 0;
    virtual bool operator()(bool value) = 0;
    virtual ~IDigiOut(){}
};

struct DigInput
{
    bool value = false;
    event::Event<bool> Changed;
};

struct TemperatureSensor
{
    float value;
    event::Event<float> Changed;
};
