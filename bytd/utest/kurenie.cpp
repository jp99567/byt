#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

namespace kurenie {
class KurenieTest
{
public:
  static float getDeltaT(ust roomIdx, const Kurenie& k)
  {
    return k.t_cur[roomIdx].delta();
  }
  static float getCurT(ust roomIdx, const Kurenie& k)
  {
    return k.t_cur[roomIdx].cur();
  }
  static ust getBufSize(const Kurenie& k)
  {
    return k.t_cur[0].tcirbuf.size();
  }
};
}

TEST_CASE( "teplota", "deltaT" ) {
  auto hwUptr = std::make_unique<kurenie::HwMock>();
  auto& hw = *hwUptr;
  init(hw);
  kurenie::Kurenie k(std::move(hwUptr));

  const auto izba = kurenie::ROOM::Kuchyna;

  hw.roomt[idx(izba)] = 1.2;

  using Catch::Matchers::WithinAbsMatcher;

  for(ust i = 0; i < kurenie::KurenieTest::getBufSize(k); ++i){
    k.process(tpmin);
    CHECK_THAT( 1.2, WithinAbsMatcher(kurenie::KurenieTest::getCurT(idx(izba), k), 1e-6));
  }
  CHECK_THAT( 0.0, WithinAbsMatcher(kurenie::KurenieTest::getDeltaT(idx(izba), k), 1e-6));

  for(ust i = 0; i < kurenie::KurenieTest::getBufSize(k); ++i){
    hw.roomt[idx(izba)] += 0.1;
    k.process(tpmin);
  }
  CHECK_THAT( kurenie::KurenieTest::getBufSize(k)*0.1-0.1, WithinAbsMatcher(kurenie::KurenieTest::getDeltaT(idx(izba), k), 1e-6));

  auto expectedDT = kurenie::KurenieTest::getDeltaT(idx(izba), k);
  for(ust i = 0; i < kurenie::KurenieTest::getBufSize(k)-1; ++i){
    k.process(tpmin);
    expectedDT -= 0.1;
    auto curDT = kurenie::KurenieTest::getDeltaT(idx(izba), k);
    CHECK_THAT( expectedDT, Catch::Matchers::WithinAbsMatcher(curDT, 1e-6));
  }
  k.process(tpmin);
  CHECK_THAT( 0, Catch::Matchers::WithinAbsMatcher(kurenie::KurenieTest::getDeltaT(idx(izba), k), 1e-6));
}
