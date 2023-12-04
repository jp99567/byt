#pragma once

namespace kurenie {

enum class ROOM { Obyvka, Spalna, Kuchyna, Izba, Kupelka, Podlahovka, _last };
class IHw
{
public:
    virtual void setTch(float t) = 0;
    virtual void setTEV(ROOM room, float v) = 0;
    virtual float curTch() const = 0;
    virtual float curTroom(ROOM room) const = 0;
    virtual ~IHw(){}
};
}
