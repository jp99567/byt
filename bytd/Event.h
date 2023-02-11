#pragma once

#include "Subscription.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace event {

class IObserverConnection
{
public:
    virtual ~IObserverConnection() = 0;
    virtual std::unique_ptr<IObserverConnection> makeScopedDestructor() = 0;
};

using IObserverConnectionHandler = std::unique_ptr<IObserverConnection>;

inline IObserverConnection::~IObserverConnection() = default;

template<typename... Args>
struct IEvent
{
    virtual ~IEvent(){}
    virtual IObserverConnectionHandler subscribe(std::unique_ptr<ISubscription<void, Args...>>) = 0;
    virtual void unsubscribe(std::unique_ptr<ISubscription<void, Args...>>) = 0;
};

template<typename... Args>
class Event : public IEvent<Args...>
{
public:
    using ISubscr    = ISubscription<void, Args...>;
    using ISubscrPtr = std::unique_ptr<ISubscr>;

    struct ObserversContainer
    {
        std::mutex              lock;
        std::vector<ISubscrPtr> observers;
    };

    struct HandlerImpl : public IObserverConnection
    {
        std::weak_ptr<ObserversContainer> observers;
        ISubscr*                          thisObserver = nullptr;
        bool releaseInDestructor = false;

        explicit HandlerImpl(std::weak_ptr<ObserversContainer> observers, ISubscr* thisObserver, bool release)
            :observers(observers), thisObserver(thisObserver), releaseInDestructor(release)
        {}

        IObserverConnectionHandler makeScopedDestructor() override
        { 
            this->releaseInDestructor = false;
            auto handler = std::make_unique<HandlerImpl>(*this);
            handler->releaseInDestructor = true;
            return handler;
        }

        virtual ~HandlerImpl()
        {
            if (releaseInDestructor)
            {
                auto o = observers.lock();

                if(o)
                {
                    unsubscr(o, [this](const ISubscrPtr& uPtr) { return uPtr.get() == this->thisObserver; });
                }
            }
        }
    };

    virtual ~Event() = default;
    Event()
    {
        mObservers = std::make_shared<ObserversContainer>();
    }

    IObserverConnectionHandler subscribe (std::unique_ptr<ISubscription<void, Args...>> observer) override
    {
        auto handler = std::make_unique<HandlerImpl>(mObservers, observer.get(), false);
        std::lock_guard<std::mutex> lock(mObservers->lock);
        mObservers->observers.push_back(std::move(observer));
        return handler;
    }

    void unsubscribe (std::unique_ptr<ISubscription<void, Args...>> observer) override
    {
        unsubscr(this->mObservers, [&observer](const ISubscrPtr& uPtr) { return uPtr->isSame(*observer); });
    }

    void notify(Args... args)
    {
        std::lock_guard<std::mutex> lock(mObservers->lock);
        for(auto& obs : mObservers->observers)
            obs->raise(args...);
    }

    void operator()(Args... args)
    {
        this->notify(args...);
    }

private:

    template<typename Predicate>
    static void unsubscr(std::shared_ptr<ObserversContainer> observers, Predicate predicate)
    {
        std::lock_guard<std::mutex> lock(observers->lock);
        observers->observers.erase(
            std::remove_if(observers->observers.begin(),
                           observers->observers.end(),
                           predicate
            ),
            observers->observers.end());
    }

    std::shared_ptr<ObserversContainer> mObservers;
};

}
