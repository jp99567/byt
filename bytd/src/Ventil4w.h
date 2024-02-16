#pragma once
#include <atomic>
#include <gpiod.hpp>

class Ventil4w
{
public:
    enum class State {Kotol, StudenaVoda, Boiler, KotolBoiler, moving, error};
    Ventil4w();
    void moveTo(State state);
    State curState() const;

private:
    std::atomic<State> state;
    bool waitEvent();
    gpiod::chip chip;
    gpiod::line_bulk lines_in;
};

