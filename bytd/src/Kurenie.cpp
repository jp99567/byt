#include "Kurenie.h"
#include <algorithm>
#include <cmath>

namespace kurenie {

Kurenie::Kurenie(std::unique_ptr<IHw> hw) : hw(std::move(hw))
{
  override_t_TEV_val.fill(std::numeric_limits<float>::quiet_NaN());
}

void Kurenie::override_t_TEV(ROOM room, float pwm)
{
  override_t_TEV_val[idx(room)] = pwm;
  set_t_TEV(room);
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

void Kurenie::set_t_TEV()
{
  for(int room = 0; (ROOM)room < ROOM::_last; ++room){
    set_t_TEV((ROOM)room);
  }
}

void Kurenie::set_t_TEV(ROOM room)
{
  hw->setTEV(room, std::isnan(override_t_TEV_val[idx(room)])
                       ? t_TEV_val[idx(room)]
                       : override_t_TEV_val[idx(room)], curTP);
}
}
