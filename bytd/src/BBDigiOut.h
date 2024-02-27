#pragma once

#include "IIo.h"
#include <gpiod.hpp>

class BBDigiOut : public IDigiOut {
    gpiod::line line;

public:
    explicit BBDigiOut(gpiod::chip& chip, unsigned lineNr);
    operator bool() const override;
    bool operator()(bool value) override;
    ~BBDigiOut() override { }
};
