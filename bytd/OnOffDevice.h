#pragma once

#include <memory>

class IDigiOut;

class OnOffDevice
{
public:
    explicit OnOffDevice(std::unique_ptr<IDigiOut> out);
    void toggle();
    void set(bool on=true);
private:
    std::unique_ptr<IDigiOut> out;
};

