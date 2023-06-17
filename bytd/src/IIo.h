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
    const std::string name;
    bool value = false;
    event::Event<bool> Changed;
};

struct SensorInput
{
    const std::string name;
    float value = std::numeric_limits<float>::quiet_NaN();
    event::Event<float> Changed;
};
