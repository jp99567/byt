#pragma once

#include "Event.h"

class IDigiOut {
public:
    virtual operator bool() const = 0;
    virtual bool operator()(bool value) = 0;
    virtual ~IDigiOut() { }
};

struct DigInput {
    const std::string name;
    bool value = false;
    event::Event<bool> Changed;
};

class ISensorInput {
public:
    virtual event::Event<float>& Changed() = 0;
    virtual float value() const = 0;
    virtual std::string name() const = 0;
    virtual ~ISensorInput() { }

protected:
    virtual void update(float) = 0;
};
