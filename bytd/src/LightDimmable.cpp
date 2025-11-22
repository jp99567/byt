#include "LightDimmable.h"

namespace Light {

Dimmable::Dimmable(std::shared_ptr<DimmEvent> dimev, std::unique_ptr<IPwmDev> pwmdev)
    : dimev(std::move(dimev)), pwmdev(std::move(pwmdev))
{}

void Dimmable::setTarget(float val)
{
    if(target != val)
    {
        if(target > 0)
            lastTargetOn = target;
        target = val;
        adjust();
        if(cur_val != target)
        {
            if(not subscHandler)
                subscHandler = dimev->subscribe(*this);
        }
    }
}

void Dimmable::adjust()
{
    if(target != cur_val)
    {
        bool finished = false;
        if( target > cur_val)
        {
            cur_val += adjStep;
            if( target <= cur_val){
                cur_val = target;
                finished = true;
            }
        }
        else if(target < cur_val)
        {
            cur_val -= adjStep;
            if( target >= cur_val){
                cur_val = target;
                finished = true;
            }
        }

        (*pwmdev)(cur_val);
        if(finished){
            dimev->unsubscribe(subscHandler);
            subscHandler = nullptr;
        }
    }
}

void Dimmable::setFullRangeRampSeconds(FloatSeconds sec)
{
    auto stepNr = sec / DimmEvent::period;
    adjStep = IPwmDev::fullRange / stepNr;
}

DimmEvent::DimmEvent(boost::asio::io_service &io_context)
    :timer(io_context)
{}

void DimmEvent::schedul()
{
    timer.expires_after(period);
    timer.async_wait([this](const boost::system::error_code&) {
        event.notify();
        if(event.subsribersCount())
        {
            schedul();
        }
    });
}

DimmEventPriv::SubsrHandller DimmEvent::subscribe(Dimmable &light)
{
    if(event.subsribersCount() == 0)
    {
        schedul();
    }
    return event.subscribe(event::subscr(&light, &Dimmable::adjust));
}

void DimmEvent::unsubscribe(DimmEventPriv::SubsrHandller hdl)
{
    event.unsubscribe(hdl);
}

OnOffOutput::operator bool() const
{
    return dimmable->getTarget() > 0;
}

bool OnOffOutput::operator()(bool value)
{
    if (value)
        dimmable->setOn();
    else
        dimmable->setTarget(0);
    return value;
}
}
