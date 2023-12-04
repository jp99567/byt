#pragma once

#include "kurenieIHw.h"
#include <vector>
#include <memory>

class OpenTherm;
class slowswi2cpwm;

namespace kurenie {

class TEV;

class HW : public IHw
{
public:
    explicit HW(OpenTherm& ot, slowswi2cpwm& tevpwm);
    ~HW();

    void setTch(float t) override;
    void setTEV(ROOM room, float v) override;
    float curTch() const override;
    float curTroom(ROOM room) const override;

private:
    OpenTherm& ot;
    slowswi2cpwm& tevCtrl;
    std::vector<std::unique_ptr<TEV>> tevs;
};

} // namespace kurenie

