#include "Kurenie.h"
#include <algorithm>
#include <cmath>

namespace kurenie {

Kurenie::Kurenie(std::unique_ptr<IHw> hw) : hw(std::move(hw))
{
  override_pwm_TEV_val.fill(std::numeric_limits<float>::quiet_NaN());
}

void Kurenie::setSP(ROOM room, float t)
{
  if(std::isnan(t))
    t = 0;
  roomSP[idx(room)] = t;
}

void Kurenie::process(std::chrono::steady_clock::time_point tp)
{
  curTP = tp;
  calc();
  check();
  update();
}

void Kurenie::override_pwm_TEV(ROOM room, float pwm)
{
  override_pwm_TEV_val[idx(room)] = pwm;
  set_pwm_TEV(room);
}

void Kurenie::override_t_CH(float t)
{
  override_t_CH_val = t;
  set_t_CH();
}

void Kurenie::set_t_CH()
{
  hw->setTch(std::isnan(override_t_CH_val) ? t_CH : override_t_CH_val);
}

void Kurenie::set_pwm_TEV()
{
  for (int room = 0; (ROOM)room < ROOM::_last; ++room) {
    set_pwm_TEV((ROOM)room);
  }
}

void Kurenie::set_pwm_TEV(ROOM room)
{
  hw->setTEV(room,
             std::isnan(override_pwm_TEV_val[idx(room)])
             ? pwm_TEV_val[idx(room)]
                 : override_pwm_TEV_val[idx(room)],
             curTP);
}

void Kurenie::calc()
{
  if(std::any_of(std::cbegin(roomSP), std::cbegin(roomSP),
                  [](auto sp) { return sp > 0; })) {
  }
  else {
    t_CH = 0;
  }
}

void Kurenie::check()
{
}

void Kurenie::update()
{
  set_pwm_TEV();
  set_t_CH();
}

void Kurenie::setError()
{

}
}
