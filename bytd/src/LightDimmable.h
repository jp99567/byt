#pragma once
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include "Event.h"
#include "IPwmDev.h"
#include "IIo.h"

namespace Light {

class Dimmable;

using DimmEventPriv = event::EventSimpleSt<>;
class DimmEvent
{
    DimmEventPriv event;
    boost::asio::steady_timer timer;
public:
    constexpr static auto period = std::chrono::milliseconds(40);
    explicit DimmEvent(boost::asio::io_service& io_context);
    DimmEventPriv::SubsrHandller subscribe(Dimmable& light);
    void unsubscribe(DimmEventPriv::SubsrHandller hdl);

private:
    void schedul();
};

using FloatSeconds = std::chrono::duration<float>;

class Dimmable
{
    float cur_val = 0;
    float target = 0;
    float lastTargetOn = 50;
    float adjStep = 1;
    bool adjusting = false;
    std::shared_ptr<DimmEvent> dimev;
    std::unique_ptr<IPwmDev> pwmdev;
public:
    Dimmable(std::shared_ptr<DimmEvent> dimev, std::unique_ptr<IPwmDev> pwmdev);
    DimmEventPriv::SubsrHandller subscHandler = nullptr;
    void setTarget(float val);
    void adjust();
    void setFullRangeRampSeconds(FloatSeconds sec);
    const float getTarget() const {return target;}
    void setOn()
    {
        setTarget(lastTargetOn);
    }
};

using DimmableSptr = std::shared_ptr<Dimmable>;

class OnOffOutput : public IDigiOut
{
public:
    explicit OnOffOutput(DimmableSptr dimmable):dimmable(std::move(dimmable)){}
    operator bool() const override;
    bool operator()(bool value) override;
private:
    DimmableSptr dimmable;
};

}
