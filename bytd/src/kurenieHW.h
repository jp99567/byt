#pragma once

#include "IMqttPublisher.h"
#include "kurenieIHw.h"
#include <memory>
#include <vector>
#include <array>

class OpenTherm;
class slowswi2cpwm;
class ISensorInput;

namespace kurenie {

class TEV;

class HW : public IHw {
public:
    using SensorsT = std::vector<std::reference_wrapper<ISensorInput>>;
    explicit HW(OpenTherm& ot, slowswi2cpwm& tevpwm, IMqttPublisherSPtr mqtt, SensorsT sensT, SensorsT podlahaPrivodT);
    ~HW();

    void setTch(float t) override;
    void setTEV(ROOM room, float pwm, Clock::time_point tp) override;
    void setTEVsDefault(Clock::time_point tp) override;
    float curTch() const override;
    float curTroom(ROOM room) const override;
    float curT_podlahaPrivod() const override;
    bool isOpened(ROOM room, Clock::time_point tp) const override;
    bool isClosed(ROOM room, Clock::time_point tp) const override;

private:
    struct CurT {
        int enr = 0;
        float v = 25;
        void update(const float value);
    };
    OpenTherm& ot;
    slowswi2cpwm& tevCtrl;
    IMqttPublisherSPtr mqtt;
    std::vector<std::unique_ptr<TEV>> tevs;
    std::array<CurT, roomNr> curTemp;
    CurT podlahaPrivod;
};

} // namespace kurenie
