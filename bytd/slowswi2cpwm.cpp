#include "slowswi2cpwm.h"
#include "thread_util.h"
#include <cmath>

namespace  {

constexpr auto mints = std::chrono::milliseconds(10);
constexpr unsigned nrts = 100;
constexpr auto period = mints*nrts;

std::chrono::milliseconds calc_dur(float pwm)
{
    unsigned count = std::lround(nrts * pwm/100.0);
    return mints * count;
}

constexpr unsigned map[8] = { 1, 0, 2, 3, 4, 5, 6, 7};

}

slowswi2cpwm::slowswi2cpwm()
{
    t = std::thread([this]{
        thread_util::sigblock(true, true);
        thread_util::set_thread_name("bytd-i2cpwm");
        auto to = clk::now() + period;
        clk::time_point next;
        while(not fini){
            std::unique_lock<std::mutex> lock(lck);
            uint8_t newval = 0;
            if(cv.wait_until(lock, to, [this,&newval]{
                if(fini)
                   return true;
                newval = calc_new_val();
                return newval != last_value;
            })){
                lock.unlock();
                update(newval);
            }
            else{ //timeout
                using namespace std::chrono_literals;
                bool isT0 = schedule.empty();
                if(isT0){
                    next = to + period;
                    pwm_bits = 0;
                    for(unsigned idx = 0; idx < pwm.size(); ++idx){
                        auto& ch = pwm[idx];
                        auto width = calc_dur(std::get<0>(ch));
                        auto& chto = std::get<1>(ch);
                        if(width == period){
                            pwm_bits |= 1 << map[idx];
                            chto = clk::time_point::max();
                        }
                        else if(width > 0ms){
                            pwm_bits |= 1 << map[idx];
                            chto = to + width;
                            schedule.emplace_back(chto);
                        }
                    }
                }
                else{
                    for(unsigned idx = 0; idx < pwm.size(); ++idx){
                        auto& ch = pwm[idx];
                        auto& chto = std::get<1>(ch);
                        if(to > chto)
                            pwm_bits &= ~(1 << map[idx]);
                    }
                }

                if(schedule.empty()){
                    to = next;
                }
                else{
                    to = schedule.back();
                    schedule.pop_back();
                }

                newval = calc_new_val();
                if(newval != last_value){
                    lock.unlock();
                    update(newval);
                }
            }
        }

    });
}

slowswi2cpwm::~slowswi2cpwm()
{
    {
        std::lock_guard<std::mutex> lock(lck);
        fini = true;
        cv.notify_all();
    }
    t.join();
}

void slowswi2cpwm::update(unsigned idx, float value)
{
    if(idx >= pwm.size())
        throw std::out_of_range("slowswi2cpwm index");

    if(value > 100 || value < 0)
        throw std::out_of_range("slowswi2cpwm value");

    std::lock_guard<std::mutex> lock(lck);
    std::get<0>(pwm[idx]) = value;
}

void slowswi2cpwm::dig1Out(bool v)
{
    std::lock_guard<std::mutex> lock(lck);
    d1 = v;
    cv.notify_all();
}

void slowswi2cpwm::dig2Out(bool v)
{
    std::lock_guard<std::mutex> lock(lck);
    d2 = v;
    cv.notify_all();
}

void slowswi2cpwm::update(uint8_t val)
{
    last_value = val;
    out.value(last_value);
}

uint8_t slowswi2cpwm::calc_new_val() const
{
    uint8_t bits = pwm_bits;
    if(d1)
        bits |= 1 << map[6];
    if(d2)
        bits |= 1 << map[7];
    return bits;
}
