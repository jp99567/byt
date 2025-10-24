#include "Plynomer.h"
#include "IIo.h"

Plynomer::Plynomer(IMqttPublisher& mqtt, DigInput& digIn)
    :ImpulzyBase(mqtt, "plynomer_total_m3")
{
    digIn.Changed.subscribe(event::subscr([this](bool val) {
        event(val ? EventType::rising : EventType::falling);
    }));
}

constexpr double imp_per_m3 = 0.01;

float Plynomer::calc() const
{
    return orig + impCount / imp_per_m3;
}
