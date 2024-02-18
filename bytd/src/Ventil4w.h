#pragma once
#include <atomic>
#include <gpiod.hpp>
#include <functional>
#include "IMqttPublisher.h"

class Ventil4w
{
public:
    using powerFnc = std::function<void(bool)>;
    Ventil4w(powerFnc power, IMqttPublisherSPtr mqtt);
    void moveTo(std::string target);

  private:
    bool waitEvent(std::chrono::steady_clock::time_point deadline);

    template<typename _Rep, typename _Period>
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
    gpiod::chip chip;
    gpiod::line_bulk lines_in;
    gpiod::line_bulk lines_out;
    int ev_line_port_mark = 0;
    int ev_line_abs_mark = 0;
    powerFnc power;
    bool in_position = false;
    int cur_position = 0;
    std::atomic_flag moving;
    IMqttPublisherSPtr mqtt;
};

