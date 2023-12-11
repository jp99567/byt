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
  void process(Clock::time_point tp);
  void override_pwm_TEV(ROOM room, float pwm);
  void override_t_CH(float t);
  static constexpr auto dT = std::chrono::seconds(20);

private:
  ROOM mainRoom = ROOM::Obyvka;
  float t_CH = 0;
  float t_CH_lim_low = 38;
  float t_CH_lim_high = 80;
  float override_t_CH_val = std::numeric_limits<float>::quiet_NaN();
  std::array<float, roomNr> pwm_TEV_val;
  std::array<float, roomNr> override_pwm_TEV_val;
  std::array<float, roomNr> roomSP;
  struct CurT {
    static constexpr std::size_t cnt = 6;
    std::array<float,cnt> tcirbuf;
    std::size_t curIdx = 0;
    void add(float val);
    float delta() const;
    float cur() const;
  };
  std::array<CurT, roomNr> t_cur;
  enum class Reg { under, in, over };

  bool error = false;
  bool tevs_set_default = false;
  void set_t_CH();
  void set_pwm_TEV();
  void set_pwm_TEV(ROOM room);
  void calc();
  void check();
  void update();
  void setError();
  void valveControl(ROOM room);
  void mainControl();
  void update_t_bufs();
  Clock::time_point curTP = Clock::time_point::min();
  static constexpr float in_reg_range = 0.75;
};
} // namespace kurenie
