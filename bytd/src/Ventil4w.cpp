#include "Ventil4w.h"
#include <iostream>
#include <thread>

constexpr unsigned line_port_mark = 7;
constexpr unsigned line_abs_mark = 9;
constexpr unsigned line_dir_p = 11;
constexpr unsigned line_dir_n = 13;

constexpr std::string_view statusTxt[] = {"Kotol", "StudenaVoda", "Boiler", "KotolBoiler"};

constexpr std::chrono::seconds nextPortTimeout(1);
constexpr std::chrono::milliseconds exitPositionTimeout(400);

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

Ventil4w::Ventil4w(powerFnc power)
    : chip("2"), lines_in(chip.get_lines({line_port_mark, line_abs_mark})),
      lines_out(chip.get_lines({line_dir_p, line_dir_n})),
      power(power)
{
  lines_in.request({"bytd-ventil", gpiod::line_request::EVENT_BOTH_EDGES, 0});
  lines_out.request({"bytd-ventil", gpiod::line_request::DIRECTION_OUTPUT, 0},{0,0});
  std::cout << "line0: " << lines_in[0].get_value() << '\n';
  std::cout << "line1: " << lines_in[1].get_value() << '\n';
}

bool Ventil4w::waitEvent(unsigned int ms)
{
    ev_line_port_mark = 0;
    ev_line_abs_mark = 0;
    auto lswe = lines_in.event_wait(std::chrono::nanoseconds(ms*1000000));
    std::cout << "empty:" << lswe.empty() << "size:" << lswe.size() << "\n";
    for(unsigned i=0; i<lswe.size(); ++i){
      auto e = lswe[i].event_read();
      if(e.source.offset() == line_abs_mark)
        ev_line_abs_mark = e.event_type;
      if(e.source.offset() == line_port_mark)
        ev_line_port_mark = e.event_type;
    }
    return not lswe.empty();
}

bool Ventil4w::home_pos()
{
  power(true);
  ScopedExit poweroff([this]{power(false);});
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  flush_spurios_signals();
  bool absMarkReached = absMark();
  const auto abs_deadline = std::chrono::steady_clock::now() + 5 * nextPortTimeout;
  motorStart(true);
  int dist = 0;
  int maxDistance = absMarkReached ? 1 : 5;

  while(dist < maxDistance){
    if(abs_deadline < std::chrono::steady_clock::now()){
      std::cout << "{process deadline timeout\n";
      break;
    }
    if(waitEvent(nextPortTimeout.count()*1000)){
      if(ev_line_port_mark == gpiod::line_event::RISING_EDGE)
        dist++;
      if(ev_line_abs_mark == gpiod::line_event::RISING_EDGE){
        if(not absMarkReached){
          absMarkReached = true;
          maxDistance = dist + 1;
        }
      }
    }
    else{
      std::cout << "{process timeout\n";
    }
  }

  motorStop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  in_position = portMark() && absMarkReached;
  if(in_position){
    cur_port = State::Kotol;
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
    return false;
  }

  if(not portMark()){
    return false;
  }

  auto [forwardDirection, targetDist] = calcMotion(cur_position, target);
  int inkrement = forwardDirection ? 1 : -1;
  bool posError = false;
  //print(f"check2 {targetDist} {forwardDirection} {inkrement} {self.curPort} {self.inPosition}")
  if(targetDist == 0)
    return true;

  const auto abs_deadline = std::chrono::steady_clock::now() + targetDist * nextPortTimeout;
  motorStart(forwardDirection);
  bool exitPosition = false;

  if(waitEvent(exitPositionTimeout.count())){
    if(ev_line_port_mark == gpiod::line_event::FALLING_EDGE){
      exitPosition = true;
      in_position = false;
    }
  }

  int dist = 0;
  bool absMarkHit = absMark();
  bool expectAbsMark = expectedAbsMark(cur_position, forwardDirection);

  while(exitPosition && dist < targetDist){
    if(waitEvent(nextPortTimeout.count()*1000)){
      if(abs_deadline < std::chrono::steady_clock::now()){
        //print(f"{datetime.now()} process deadline timeout {dist}");
        break;
      }
      if(ev_line_port_mark == gpiod::line_event::RISING_EDGE){
        dist += 1;
        cur_position = (cur_position + inkrement + 4) % 4;
        if(expectAbsMark != absMarkHit){
          posError = true;
          break;
        }
        expectAbsMark = expectedAbsMark(cur_position, forwardDirection);
        absMarkHit = false;
      }
      if(ev_line_abs_mark == gpiod::line_event::RISING_EDGE)
        absMarkHit = true;
    }
    else{
      //print(f"process timeout {dist}")
      break;
    }
  }
  motorStop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if(posError)
    return false;
  if(not portMark())
    return false;

  in_position = cur_position == target;
  return in_position;
}

void Ventil4w::flush_spurios_signals()
{
  waitEvent(100);
  ev_line_port_mark = 0;
  ev_line_abs_mark = 0;
}

bool Ventil4w::absMark()
{
  return lines_in[1].get_value();
}

bool Ventil4w::portMark()
{
  return lines_in[0].get_value();
}

void Ventil4w::motorStart(bool dir)
{
  lines_out[(unsigned)dir].set_value(1);
}

void Ventil4w::motorStop()
{
  lines_out[0].set_value(0);
  lines_out[1].set_value(0);
}

