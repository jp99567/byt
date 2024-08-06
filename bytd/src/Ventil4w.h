#pragma once
#include "IMqttPublisher.h"
#include <atomic>
#include <functional>
#include <gpiod.hpp>

class Ventil4w {
public:
    using powerFnc = std::function<void(bool)>;
    Ventil4w(powerFnc power, IMqttPublisherSPtr mqtt);
    void moveTo(std::string target);

private:
    bool waitEvent(std::chrono::steady_clock::time_point deadline);

    template <typename _Rep, typename _Period>
    bool waitEvent(const std::chrono::duration<_Rep, _Period>& timeout)
    {
        return waitEvent(std::chrono::steady_clock::now() + timeout);
    }

    void flush_spurios_signals();
    bool absMark();
    bool portMark();
    void motorStart(bool dir);
    void motorStop();
    bool home_pos();
    bool move(const int target);
    void task(int target_positon);
    void lostPosition();
    void loadPosition();
    gpiod::chip chip;
    gpiod::line_bulk lines_in;
    gpiod::line line_out_p;
    gpiod::line line_out_n;
    int ev_line_port_mark = 0;
    int ev_line_abs_mark = 0;
    powerFnc power;
    bool in_position = false;
    int cur_position = 0;
    std::atomic_flag moving;
    IMqttPublisherSPtr mqtt;
    bool flush_gpio_events_flag = false;
};
