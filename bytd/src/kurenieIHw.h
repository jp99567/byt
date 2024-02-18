#pragma once

#include <chrono>
#include "TxtEnum.h"

namespace kurenie {

using Clock = std::chrono::steady_clock;

class IHw
{
public:
    virtual void setTch(float t) = 0;
    virtual void setTEV(ROOM room, float pwm, Clock::time_point tp) = 0;
    virtual void setTEVsDefault(Clock::time_point tp) = 0;
    virtual float curTch() const = 0;
    virtual float curTroom(ROOM room) const = 0;
    virtual float curT_podlahaPrivod() const = 0;
    virtual bool isOpened(ROOM room, Clock::time_point tp) const = 0;
    virtual bool isClosed(ROOM room, Clock::time_point tp) const = 0;
    virtual ~IHw(){}
};

constexpr std::size_t idx(ROOM room){return static_cast<std::size_t>(room);}
constexpr auto roomNr = (std::size_t)ROOM::_last;

}
