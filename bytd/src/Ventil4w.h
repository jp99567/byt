#pragma once
#include <atomic>
#include <gpiod.hpp>
#include <functional>

class Ventil4w
{
public:
    using powerFnc = std::function<void(bool)>;
    enum class State {Kotol, StudenaVoda, Boiler, KotolBoiler, moving, error};
    Ventil4w(powerFnc power);
    void moveTo(State state);
    State curState() const;
    bool waitEvent(unsigned ms);
    bool home_pos();
    bool move(const int target);

  private:
    void flush_spurios_signals();
    bool absMark();
    bool portMark();
    void motorStart(bool dir);
    void motorStop();
    std::atomic<State> state;
    gpiod::chip chip;
    gpiod::line_bulk lines_in;
    gpiod::line_bulk lines_out;
    int ev_line_port_mark = 0;
    int ev_line_abs_mark = 0;
    powerFnc power;
    bool in_position = false;
    int cur_position = 0;
    State cur_port = State::Kotol;
};

