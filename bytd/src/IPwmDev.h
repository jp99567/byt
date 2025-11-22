#pragma once

class IPwmDev
{
public:
    constexpr static float fullRange = 100.0;
    virtual operator float() const = 0;
    virtual float operator()(float value) = 0;
    virtual ~IPwmDev() { }
};
