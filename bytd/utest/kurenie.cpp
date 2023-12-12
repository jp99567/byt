#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bitset>

#include "../src/Kurenie.h"
#include "../src/kurenieIHw.h"

namespace kurenie {
class HwMock : public IHw
{
public:
  void setTch(float t) override { ch_t_sp = t;}
  void setTEV(ROOM room, float pwm, Clock::time_point)  override { tevpwm[idx(room)] = pwm;}
  void setTEVsDefault(Clock::time_point)  override
  {
    tevpwm.fill(100);
    tevpwm[idx(ROOM::Podlahovka)] = 0;
  }
  float curTch() const override {return cur_t_ch;}
  float curTroom(ROOM room) const  override {return roomt[idx(room)];}
  bool isOpened(ROOM room, Clock::time_point) const  override {return opened[idx(room)];}

  std::array<float, roomNr> roomt;
  std::array<float, roomNr> tevpwm;
  std::bitset<roomNr> opened;
  float cur_t_ch = 25;
  float ch_t_sp = 0;
};
}

auto tpmin = kurenie::Clock::time_point::min();
void init(kurenie::HwMock& hw)
{
  hw.cur_t_ch = 20;
  hw.ch_t_sp = 0;
  hw.opened.reset();
  hw.setTEVsDefault(tpmin);
  hw.roomt.fill(20);
}

TEST_CASE( "Kurenie", "simple case 1" ) {
  auto hwUptr = std::make_unique<kurenie::HwMock>();
  auto& hw = *hwUptr;
  init(hw);
  kurenie::Kurenie k(std::move(hwUptr));

  k.process(tpmin);
  using kurenie::ROOM;
  CHECK(hw.ch_t_sp == 0);
  for(ust ir=0; ir < idx(ROOM::Podlahovka); ++ir){
    CHECK(hw.tevpwm[ir] == 100);
  }
  CHECK(hw.tevpwm[idx(ROOM::Podlahovka)] == 0);

  for(ust ir=0; ir < idx(ROOM::Podlahovka); ++ir){
    hw.opened.set(ir,true);
  }
  k.process(tpmin);
  for(ust ir=0; ir < idx(ROOM::Podlahovka); ++ir){
    CHECK(hw.tevpwm[ir] == 100);
  }
  k.setSP(ROOM::Kuchyna, 25);
  k.process(tpmin);
  CHECK(hw.tevpwm[idx(ROOM::Kuchyna)] == 100);
  CHECK(hw.tevpwm[idx(ROOM::Izba)] == 0);
}
