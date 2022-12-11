#pragma once

#include<array>
#include<vector>
#include<chrono>
#include<thread>
#include<condition_variable>

#include "I2cDiOut.h"

class slowswi2cpwm
{
    using clk = std::chrono::steady_clock;
    std::array<std::tuple<float,clk::time_point>,6> pwm;
    std::vector<clk::time_point> schedule;
    uint8_t pwm_bits = 0;
    uint8_t last_value = 0;
    int ph = 0;
    bool d1 = false;
    bool d2 = false;
    std::thread t;
    bool fini = false;
    std::mutex lck;
    std::condition_variable cv;
    I2CDiOut out;
public:
    slowswi2cpwm();
    ~slowswi2cpwm();
    void update(unsigned idx, float value);
    void dig1Out(bool v);
    void dig2Out(bool v);
private:
    void update(uint8_t val);
    uint8_t calc_new_val() const;
};

