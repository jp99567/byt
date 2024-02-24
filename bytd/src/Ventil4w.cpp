#include "Ventil4w.h"
#include "Log.h"
#include <thread>
#include <boost/algorithm/string.hpp>
#include <fstream>

constexpr unsigned line_port_mark = 9;
constexpr unsigned line_abs_mark = 7;
constexpr unsigned line_dir_p = 11;
constexpr unsigned line_dir_n = 13;

constexpr std::string_view positionTxt[] = {"Kotol", "StudenaVoda", "Boiler", "KotolBoiler"};

constexpr std::chrono::milliseconds nextPortTimeout(700);
constexpr std::chrono::milliseconds exitPositionTimeout(150);

class ScopedExit
{
  std::function<void()> onExit;
public:
  ScopedExit(std::function<void()> f): onExit(f){}
  ~ScopedExit()
  {
    onExit();
  }
};

std::pair<bool, int> calcMotion(int cur, int dst)
{
  auto dist = dst - cur;
  auto rev = dist > 0;
  if(std::abs(dist) > 2){
    rev = !rev;
    dist = 1;
  }
  return std::make_pair(rev, abs(dist));
}


// forward: AbsMArk -> K -> Voda -> B -> KB -> AbsMArk
bool expectedAbsMark(int curPos, bool fwDIR)
{
  if( curPos == 3 && fwDIR)
    return true;

  if( curPos == 0 && not fwDIR)
    return true;

  return false;
}

constexpr char last_position_file[] = "ventil4way-last-position";

#ifndef BYTD_SIMULATOR
Ventil4w::Ventil4w(powerFnc power, IMqttPublisherSPtr mqtt)
    : chip("2")
    , lines_in(chip.get_lines({line_port_mark, line_abs_mark}))
    , line_out_p(chip.get_line(line_dir_p))
    , line_out_n(chip.get_line(line_dir_n))
    , power(power)
    , mqtt(std::move(mqtt))
{
  lines_in.request({"bytd-ventil", gpiod::line_request::EVENT_BOTH_EDGES, 0});
  line_out_p.request({"bytd-ventil", gpiod::line_request::DIRECTION_OUTPUT, 0}, 0);
  line_out_n.request({"bytd-ventil", gpiod::line_request::DIRECTION_OUTPUT, 0}, 0);
  LogINFO("line0: {} {} {}", line_out_p.name(), line_out_p.offset(), line_out_p.direction());
  LogINFO("line1: {} {} {}", line_out_n.name(), line_out_n.offset(), line_out_n.direction());
#else
Ventil4w::Ventil4w(powerFnc power, IMqttPublisherSPtr mqtt)
    : power(power),
      mqtt(std::move(mqtt))
{
#endif
  moving.clear();

  try{
    std::ifstream f((std::string(last_position_file)));
    std::string postxt;
    f >> postxt;
    auto pos = std::find_if(std::cbegin(positionTxt), std::cend(positionTxt), [&postxt](auto& v){return boost::iequals(postxt, v);});
    if(pos == std::cend(positionTxt)){
      LogERR("Ventil4w invalid stored position {}", postxt);
      lostPosition();
    }
    else{
      in_position = true;
      cur_position = std::distance(std::cbegin(positionTxt), pos);
      LogINFO("Ventil4w loaded position {} ({})", postxt, cur_position);
    }
  }
  catch(std::exception& e){
    LogERR("load last ventil position: {}", e.what());
  }
}

void Ventil4w::moveTo(std::string target)
{
  auto pos = std::find_if(std::cbegin(positionTxt), std::cend(positionTxt), [&target](auto& v){return boost::iequals(target, v);});
  int target_pos = 0;
  if(pos == std::cend(positionTxt)){
    try {
      target_pos = std::stoi(target);
      if(target_pos < 0 || target_pos >=  std::distance(std::cbegin(positionTxt), std::cend(positionTxt))){
        LogERR("Ventil4w invalid target position {} {}", target, target_pos);
        return;
      }
    }
    catch(std::exception& e){
      LogERR("Ventil4w invalid target position {} {}", target, e.what());
      return;
    }
  }
  else{
    target_pos = std::distance(std::cbegin(positionTxt), pos);
  }

  LogDBG("Ventil4w check {} {}", target, target_pos);

  if(target_pos == cur_position && in_position){
    LogINFO("Ventil4w already in position {}", target);
    return;
  }

    if (not moving.test_and_set()) {
      std::thread([this, target_pos] {
         task(target_pos);
         moving.clear();
      }).detach();
    }
}

bool Ventil4w::waitEvent(std::chrono::steady_clock::time_point deadline)
{
  LogINFO("waitEvent absMark:{} portMark:{}", absMark(), portMark());
    ev_line_port_mark = 0;
    ev_line_abs_mark = 0;
#ifndef BYTD_SIMULATOR
    auto lswe =
        lines_in.event_wait(std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-std::chrono::steady_clock::now()));
    for (unsigned i = 0; i < lswe.size(); ++i) {
      auto e = lswe[i].event_read();
      LogINFO("Ventil4w gpio event({}) source:{} type:{}", not flush_gpio_events_flag, e.source.offset(), e.event_type);
      if (not flush_gpio_events_flag) {
        if (e.source.offset() == line_abs_mark)
          ev_line_abs_mark = e.event_type;
        if (e.source.offset() == line_port_mark)
          ev_line_port_mark = e.event_type;
      }
    }
    return not lswe.empty();
#else
    std::this_thread::sleep_until(deadline);
    return false;
#endif
}

bool Ventil4w::home_pos()
{
  power(true);
  ScopedExit poweroff([this]{power(false);});
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  flush_spurios_signals();
  bool absMarkReached = absMark();
  const auto abs_deadline = std::chrono::steady_clock::now() + 5 * nextPortTimeout;
  int dist = 0;
  int maxDistance = absMarkReached ? 1 : 5;

  motorStart(true);
  while(dist < maxDistance){
    if(abs_deadline < std::chrono::steady_clock::now()){
      LogERR("Ventil4w deadline exceeded");
      break;
    }
    if(waitEvent(nextPortTimeout)){
      if(ev_line_port_mark == gpiod::line_event::FALLING_EDGE)
        dist++;
      if(ev_line_abs_mark == gpiod::line_event::FALLING_EDGE){
        if(not absMarkReached){
          absMarkReached = true;
          maxDistance = dist + 1;
        }
      }
    }
    else{
      LogERR("Ventil4w timeouted");
      break;
    }
  }

  motorStop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  in_position = portMark() && absMarkReached;
  if(in_position){
    cur_position = 0;
    return true;
  }

  return false;
}

bool Ventil4w::move(const int target)
{
  power(true);
  ScopedExit poweroff([this]{power(false);});
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  flush_spurios_signals();

  if(absMark()){
    LogERR("Ventil4w unexpexted abs mark indicated");
    return false;
  }

  if(not portMark()){
    LogERR("Ventil4w unexpexted port mark cleared");
    return false;
  }

  auto [forwardDirection, targetDist] = calcMotion(cur_position, target);
  int inkrement = forwardDirection ? 1 : -1;
  bool posError = false;

  if(targetDist == 0)
    return true;

  const auto abs_deadline = std::chrono::steady_clock::now() + targetDist * nextPortTimeout;
  motorStart(forwardDirection);
  bool exitPosition = false;

  auto exitPositionDeadline = std::chrono::steady_clock::now() + exitPositionTimeout;
  while(not exitPosition){
    if(waitEvent(exitPositionDeadline)){
      if(ev_line_port_mark == gpiod::line_event::RISING_EDGE){
        exitPosition = true;
        break;
      }
    }
    else{
      break;
    }
  }

  int dist = 0;
  bool absMarkHit = absMark();
  bool expectAbsMark = expectedAbsMark(cur_position, forwardDirection);

  while(exitPosition && dist < targetDist){
    if(waitEvent(nextPortTimeout)){
      if(abs_deadline < std::chrono::steady_clock::now()){
        LogERR("Ventil4w deadline exceeded");
        break;
      }
      if(ev_line_port_mark == gpiod::line_event::FALLING_EDGE){
        dist += 1;
        cur_position = (cur_position + inkrement + 4) % 4;
        if(expectAbsMark != absMarkHit){
          posError = true;
          break;
        }
        expectAbsMark = expectedAbsMark(cur_position, forwardDirection);
        absMarkHit = false;
      }
      if(ev_line_abs_mark == gpiod::line_event::FALLING_EDGE)
        absMarkHit = true;
    }
    else{
      LogERR("Ventil4w timeouted dist:{}", dist);
      break;
    }
  }
  motorStop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if(posError || not portMark() || cur_position != target){
    LogERR("exitPosition:{} posError: {} portMark:{} absMArk:{} dist:{} cur_position:{}",
           exitPosition, posError, portMark(), absMark(), dist, cur_position);
    return false;
  }

  in_position = cur_position == target;
  return in_position;
}

void Ventil4w::task(int target_pos)
{
  LogINFO("Ventil4w::task started {}", target_pos);

  if (target_pos != cur_position || not in_position) {
    mqtt->publish(mqtt::ventil4w_position, "moving");
    if (not in_position) {
      LogERR("Ventil4w not in position, homing");
      if (home_pos()) {
        if(target_pos == cur_position){
          mqtt->publish(mqtt::ventil4w_position, std::string(positionTxt[cur_position]));
        }
      }
      else{
        LogERR("Ventil4w homing failed");
        mqtt->publish(mqtt::ventil4w_position, "error");
        lostPosition();
        return;
      }
    }

    if (in_position && target_pos != cur_position) {
      LogINFO("Ventil4w::move from {} to {}", cur_position, target_pos);
      if (move(target_pos)) {
        mqtt->publish(mqtt::ventil4w_position, std::string(positionTxt[cur_position]));
      }
      else{
        LogERR("Ventil4w move to position {} failed", positionTxt[target_pos]);
        mqtt->publish(mqtt::ventil4w_position, "error");
        lostPosition();
        return;
      }
    }

    if (not in_position) {
      mqtt->publish(mqtt::ventil4w_position, "error");
    }
    else{
      std::ofstream f((std::string(last_position_file)));
      f << positionTxt[target_pos] << '\n';
    }
  }

  LogINFO("Ventil4w positioning finished {} {}", in_position, cur_position);
}

void Ventil4w::lostPosition()
{
  ::unlink(last_position_file);
  in_position = false;
}

void Ventil4w::flush_spurios_signals()
{
  flush_gpio_events_flag = true;
  waitEvent(std::chrono::milliseconds(100));
  ev_line_port_mark = 0;
  ev_line_abs_mark = 0;
  flush_gpio_events_flag = false;
}

bool Ventil4w::absMark()
{
#ifndef BYTD_SIMULATOR
  return not lines_in[1].get_value();
#else
  return false;
#endif
}

bool Ventil4w::portMark()
{
#ifndef BYTD_SIMULATOR
  return not lines_in[0].get_value();
#else
  return false;
#endif
}

void Ventil4w::motorStart(bool dir)
{
  LogINFO("motorStart {}", dir);
#ifndef BYTD_SIMULATOR
  if(dir)
    line_out_p.set_value(1);
  else
    line_out_n.set_value(1);

#endif
}

void Ventil4w::motorStop()
{
#ifndef BYTD_SIMULATOR
  line_out_p.set_value(0);
  line_out_n.set_value(0);
#endif
  LogINFO("motorStop");
}

