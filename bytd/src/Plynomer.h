#pragma once

#include "ImpulzyBase.h"
class DigInput;

class Plynomer : public ImpulzyBase {
public:
    Plynomer(IMqttPublisher &mqtt, DigInput& digIn);
private:
    float calc() const override;
};

