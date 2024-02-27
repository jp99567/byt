#pragma once

#include "IIo.h"
#include "slowswi2cpwm.h"

class DigiOutI2cExpander : public IDigiOut {
    slowswi2cpwm& ioexpander;
    void (slowswi2cpwm::*pDigOut)(bool);
    bool cachedVal = false;

public:
    DigiOutI2cExpander(slowswi2cpwm& ioexpander, int idx);

    operator bool() const override;

    bool operator()(bool value) override;
};
