#include "kurenieHW.h"
#include "OpenTherm.h"
#include "slowswi2cpwm.h"

namespace kurenie {

class TEV
{
protected:
    float value;
    explicit TEV(float val):value(val){}
};

class TEVno : public TEV
{
public:
    explicit TEVno():TEV(100){}
};

class TEVnc : public TEV
{
public:
    explicit TEVnc():TEV(0){}
};

HW::HW(OpenTherm &ot, slowswi2cpwm &tevpwm):ot(ot), tevCtrl(tevpwm)
{

}

HW::~HW()
{

}

void HW::setTch(float t)
{
    ot.chSetpoint = t;
}

void HW::setTEV(ROOM room, float v)
{

}

float HW::curTch() const
{
    return 0;
}

float HW::curTroom(ROOM room) const
{
    return 0;
}

} // namespace kurenie
