#pragma once

#include <gpiod.hpp>
#include "IIo.h"

class BBDigiOut : public IDigiOut
{
    gpiod::line line;

public:
    explicit BBDigiOut(gpiod::chip& chip, unsigned lineNr);
    operator bool() const override;
    bool operator()(bool value) override;
    ~BBDigiOut() override {}
};
