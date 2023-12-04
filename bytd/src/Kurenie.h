#pragma once

#include "kurenieIHw.h"

namespace kurenie {

class IHw;

class Kurenie
{
    IHw& hw;
public:
    explicit Kurenie(IHw& hw);
    void setSP(ROOM room, float t);
    void calc();
};
}
