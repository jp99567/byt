#pragma once

#include "kurenieIHw.h"
#include <array>
#include <memory>

namespace kurenie {

class IHw;

class Kurenie {
  std::unique_ptr<IHw> hw;

public:
  explicit Kurenie(std::unique_ptr<IHw> hw);
  void setSP(ROOM room, float t);
  void calc();
  void override_t_TEV(ROOM room, float pwm);
  void override_t_CH(float t);
  static constexpr auto dT = std::chrono::seconds(20);

private:
  ROOM mainRoom = ROOM::Obyvka;
  float t_CH = 0;
  float override_t_CH_val = std::numeric_limits<float>::quiet_NaN();
  std::array<float, roomNr> t_TEV_val;
  std::array<float, roomNr> override_t_TEV_val;
  void set_t_CH();
  void set_t_TEV();
  void set_t_TEV(ROOM room);
  Clock::time_point curTP = Clock::time_point::min();
};
} // namespace kurenie
