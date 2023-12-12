#include "Kurenie.h"
#include <algorithm>
#include <cmath>
#include "Log.h"

namespace kurenie {

Kurenie::Kurenie(std::unique_ptr<IHw> hw) : hw(std::move(hw))
{
  override_pwm_TEV_val.fill(std::numeric_limits<float>::quiet_NaN());
  for(auto& tstruct : t_cur){
    tstruct.tcirbuf.fill(25);
  }
  roomSP.fill(0);
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
  dump_state();
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

Kurenie::Reg Kurenie::room_regulation_state(ROOM room) const
{
  const auto sp = roomSP[idx(room)];
  const auto& tstruct = t_cur[idx(room)];
  auto t = tstruct.cur();
  auto e = sp - t;

  if( e > in_reg_range )
    return Reg::under;

  if( e < -in_reg_range)
    return Reg::over;

  return Reg::in;
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

constexpr bool roomExists(ROOM room)
{
  return room != ROOM::_last;
}

constexpr bool roomExists(ust room)
{
  return roomExists((ROOM)room);
}

void Kurenie::calc()
{
  tevs_set_default = false;
  update_t_bufs();

  auto mainRoomPosibleCandid = ROOM::_last;
  auto mainRoomReqCandid = ROOM::_last;
  auto is_room_under_regband = [this](ROOM room){ return Reg::under == room_regulation_state(room); };
  auto is_room_over_regband = [this](ROOM room){ return Reg::over == room_regulation_state(room); };

  for(ust ir = 0; ir < idx(ROOM::_last); ++ir){
    if(mainRoom == (ROOM)ir)
        continue;
    if(roomSP[ir] > 0){
      if(hw->isOpened((ROOM)ir, curTP)) {
            mainRoomPosibleCandid = (ROOM)ir;
          if(is_room_under_regband((ROOM)ir)){
             mainRoomReqCandid = (ROOM)ir;
          }
      }
    }
  }

  if(roomExists(mainRoom)){
    if(is_room_under_regband(mainRoom)){
      calc_rooms();
      return;
    }
  }

  if(roomExists(mainRoomReqCandid)){
    mainRoom = mainRoomReqCandid;
    calc_rooms();
    return;
  }

  if(!roomExists(mainRoom) || is_room_over_regband(mainRoom))
  {
    if(roomExists(mainRoomPosibleCandid)){
      mainRoom = mainRoomPosibleCandid;
      calc_rooms();
      return;
    }
  }
  else{
    calc_rooms();
    return;
  }

  tevs_set_default = true;
  t_CH = 0;
}

void Kurenie::calc_rooms()
{
  for(ust ir = 0; ir < idx(ROOM::_last); ++ir){
    if((ROOM)ir != mainRoom)
      valveControl((ROOM)ir);
  }
  mainControl();
}

void Kurenie::check()
{
  if(roomSP[idx(ROOM::Podlahovka)] > 0){
    t_CH_lim_high = t_CH_lim_high_podlahovka;
    if(t_CH > t_CH_lim_high)
      t_CH = t_CH_lim_high;
  }
  else{
    t_CH_lim_high = t_CH_lim_high_standard;
  }
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
  if( e > in_reg_range ){
    pwm = 100;
  }
  else if(e < -in_reg_range ){
    pwm = 0;
  }
  else{
    pwm += e*cProp - d * cDeriv;
  }

  if(pwm > 100)
    pwm = 100;
  else if(pwm < 0)
    pwm = 0;
}

void Kurenie::mainControl()
{
  constexpr float cProp = 0.1;
  constexpr float cDeriv = 7;
  pwm_TEV_val[idx(mainRoom)] = 100;
  auto sp = roomSP[idx(mainRoom)];
  auto& tstruct = t_cur[idx(mainRoom)];
  auto t = tstruct.cur();
  auto e = sp - t;

  if( e > in_reg_range ){
    t_CH = t_CH_lim_high;
  }
  else if(e < -in_reg_range ){
    t_CH = t_CH_lim_low;
  }
  else{
    auto d = tstruct.delta();
    t_CH += e*cProp - d * cDeriv;
  }

  if(t_CH > t_CH_lim_high)
    t_CH = t_CH_lim_high;
  else if(t_CH < t_CH_lim_low)
    t_CH = t_CH_lim_low;
}

void Kurenie::update_t_bufs()
{
  for(unsigned ir=0; ir < idx(ROOM::_last); ++ir){
    t_cur[ir].add(hw->curTroom((ROOM)ir));
  }
}

void Kurenie::dump_state() const
{
  for(ust ir=0; ir < idx(ROOM::_last); ++ir){
    auto room = (ROOM)ir;
    const auto sp = roomSP[ir];
    const auto& tstruct = t_cur[ir];
    LogDBG("kurenie {}:{}/{}/{} pwm:{} reg:{} opened:{}", roomTxt(room), sp, tstruct.cur(), tstruct.delta(), pwm_TEV_val[ir],
           (ust)room_regulation_state(room), hw->isOpened(room,curTP) );
  }
  LogDBG("kurenie main:{}: {}/{}", roomTxt(mainRoom), t_CH, hw->curTch());
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
