#include "kurenieHW.h"
#include "OpenTherm.h"
#include "Log.h"
#include "slowswi2cpwm.h"

namespace kurenie {

class TEV
{
public:
  void update(float val, slowswi2cpwm& i2c, IMqttPublisher& mqtt, Clock::time_point tp)
  {
    if(value != val){
      last = tp;
      value = val;
      i2c.update(i2cIdx, calcI2Cval(val));
      mqtt.publish(topic, val);
    }
  }
  void setToDefault(slowswi2cpwm& i2c, IMqttPublisher& mqtt, Clock::time_point tp)
  {
    update(defVal(), i2c, mqtt, tp);
  }

  bool isClosed(Clock::time_point tp) const
  {
    return value <= 0 && last + timeToFullTrip < tp;
  }

  bool isOpened(Clock::time_point tp) const
  {
    return value >= 100 && last + timeToFullTrip < tp;
  }

protected:
    explicit TEV(ROOM room)
      :i2cIdx(cIndex[(std::size_t)room])
      ,topic(roomTopic(room))
      ,last(Clock::now())
  {
  }
    virtual float calcI2Cval(float value) const = 0;
private:
    float value = std::numeric_limits<float>::quiet_NaN();
    const unsigned i2cIdx;
    const std::string topic;
    Clock::time_point last;
    constexpr static std::array<unsigned, roomNr> cIndex = {1,2,0,5,3,4};
    virtual float defVal() const = 0;
    static constexpr auto timeToFullTrip = std::chrono::minutes(8);
};

class TEVno : public TEV
{
public:
    explicit TEVno(ROOM room):TEV(room){}
private:
    float calcI2Cval(float value) const override
  {
      return 100-value;
  }
  float defVal() const override { return 100;}
};

class TEVnc : public TEV
{
public:
    explicit TEVnc(ROOM room):TEV(room){}
private:
  float calcI2Cval(float value) const override
  {
    return value;
  }
  float defVal() const override { return 0;}
};

HW::HW(OpenTherm &ot, slowswi2cpwm &tevpwm, IMqttPublisherSPtr mqtt,
       RoomSensors sensT)
    : ot(ot), tevCtrl(tevpwm), mqtt(std::move(mqtt)), roomSensor(std::move(sensT))
{
  if(roomSensor.size() != (std::size_t)ROOM::_last)
    throw std::runtime_error("invalid count of room sensors");

  auto createTev = [this](auto room, bool no){
    if((std::size_t)room != tevs.size())
      throw std::runtime_error("TEV vytvoreny mimo poradia");
    if(no)
      tevs.emplace_back(std::make_unique<TEVno>(room));
    else
      tevs.emplace_back(std::make_unique<TEVnc>(room));
  };

  for(auto room : {ROOM::Obyvka, ROOM::Spalna, ROOM::Kuchyna, ROOM::Izba, ROOM::Kupelka}){
    createTev(room, true);
    LogINFO("createTev: {}", tevs.size());
  }
  createTev(ROOM::Podlahovka, false);

  for(int i = 0; (ROOM)i < ROOM::_last; ++i){
    roomSensor[i].get().Changed().subscribe(event::subscr([this,i](float v){
      if(std::isnan(v)){
        ++curTemp[i].enr;
      }
      else{
        curTemp[i].v = v;
        curTemp[i].enr = 0;
      }
    }));
  }
}

HW::~HW()
{

}

void HW::setTch(float t)
{
  if(t != ot.chSetpoint){
    ot.chSetpoint = t;
    mqtt->publish("rb/stat/setpointCH", t);
  }
}

void HW::setTEV(ROOM room, float pwm, std::chrono::steady_clock::time_point tp) {
  if (pwm > 100)
    pwm = 100;
  else if (pwm < 0)
    pwm = 0;
  tevs.at(idx(room))->update(pwm, tevCtrl, *mqtt, tp);
}

void HW::setTEVsDefault(Clock::time_point tp)
{
  for(auto& tev : tevs)
    tev->setToDefault(tevCtrl, *mqtt, tp);
}

float HW::curTch() const
{
  return ot.sensCH().value();
}

float HW::curTroom(ROOM room) const { return curTemp[idx(room)].v; }

bool HW::isOpened(ROOM room, Clock::time_point tp) const
{
  return tevs.at(idx(room))->isOpened(tp);
}

bool HW::isClosed(ROOM room, std::chrono::steady_clock::time_point tp) const
{
  return tevs.at(idx(room))->isClosed(tp);
}

} // namespace kurenie
