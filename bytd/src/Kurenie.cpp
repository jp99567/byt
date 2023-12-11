#include "Kurenie.h"
#include <algorithm>
#include <cmath>

namespace kurenie {

Kurenie::Kurenie(std::unique_ptr<IHw> hw) : hw(std::move(hw))
{
  override_pwm_TEV_val.fill(std::numeric_limits<float>::quiet_NaN());
  for(auto& tstruct : t_cur){
    tstruct.tcirbuf.fill(25);
  }
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
  if(error){
    hw->setTch(0);
    return;
  }

  hw->setTch(std::isnan(override_t_CH_val) ? t_CH : override_t_CH_val);
}

void Kurenie::set_pwm_TEV()
{
  if(tevs_set_default || error){
    hw->setTEVsDefault(curTP);
    return;
  }

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
  std::vector<ROOM> out_of_reg_low;
  tevs_set_default = false;
  update_t_bufs();
  if(std::any_of(std::cbegin(roomSP), std::cbegin(roomSP),
                  [](auto sp) { return sp > 0; })) {

    for(unsigned ir=0; ir < idx(ROOM::Podlahovka); ++ir){
      if(roomSP[ir] > 0){
        if(hw->isOpened((ROOM)ir, curTP)){
          if(mainRoom == ROOM::_last || !hw->isOpened(mainRoom, curTP))
            mainRoom = (ROOM)ir;
        }
        else{
          valveControl((ROOM)ir);
        }
      }
    }
  }
  tevs_set_default = true;
  t_CH = 0;
}

void Kurenie::check()
{
}

void Kurenie::update()
{
  set_pwm_TEV();
  set_t_CH();
}

void Kurenie::setError() {}

void Kurenie::valveControl(ROOM room)
{
  constexpr float cProp = 1;
  constexpr float cDeriv = 1;
  auto sp = roomSP[idx(room)];
  auto& tstruct = t_cur[idx(room)];
  auto& pwm = pwm_TEV_val[idx(room)];
  auto d = tstruct.delta();
  auto t = tstruct.cur();

  auto e = sp - t;
  pwm += e*cProp - d * cDeriv;
}

void Kurenie::mainControl()
{
  constexpr float cProp = 0.1;
  constexpr float cDeriv = 7;
  auto sp = roomSP[idx(mainRoom)];
  auto& tstruct = t_cur[idx(mainRoom)];
  auto t = tstruct.cur();
  auto e = sp - t;

  if( e > in_reg_range ){
    t_CH = t_CH_lim_high;
  }
  else if(e < in_reg_range ){
    t_CH = t_CH_lim_low;
  }
  else{
    auto d = tstruct.delta();
    t_CH += e*cProp - d * cDeriv;
  }
}

void Kurenie::update_t_bufs()
{
  for(unsigned ir=0; ir < idx(ROOM::_last); ++ir){
    t_cur[ir].add(hw->curTroom((ROOM)ir));
  }
}

void Kurenie::CurT::add(float val)
{
  if(++curIdx >= cnt)
    curIdx = 0;
  tcirbuf[curIdx] = val;
}

float Kurenie::CurT::delta() const
{
  auto begin = curIdx+1;
  if(not (begin < cnt))
    begin = 0;
  return tcirbuf[curIdx] - tcirbuf[begin];
}

float Kurenie::CurT::cur() const
{
  return tcirbuf[curIdx];
}

}
