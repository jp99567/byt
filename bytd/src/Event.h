#pragma once

#include "Subscription.h"
#include <algorithm>
#include <mutex>
#include <vector>

namespace event {

class IObserverConnection {
public:
    virtual ~IObserverConnection() = 0;
    virtual std::unique_ptr<IObserverConnection> makeScopedDestructor() = 0;
};

using IObserverConnectionHandler = std::unique_ptr<IObserverConnection>;

inline IObserverConnection::~IObserverConnection() = default;

template <typename... Args>
struct IEvent {
    virtual ~IEvent() { }
    virtual IObserverConnectionHandler subscribe(std::unique_ptr<ISubscription<void, Args...>>) = 0;
    virtual void unsubscribe(std::unique_ptr<ISubscription<void, Args...>>) = 0;
    virtual void unsubscribe() = 0;
};

template <typename... Args>
struct IEventSimpleSt {
    using ISubscr = ISubscription<void, Args...>;
    virtual ~IEventSimpleSt() { }
    virtual const ISubscr* subscribe(std::unique_ptr<ISubscription<void, Args...>>) = 0;
    virtual void unsubscribe(std::unique_ptr<ISubscription<void, Args...>>) = 0;
    virtual void unsubscribe() = 0;
};

template <typename... Args>
class Event : public IEvent<Args...> {
public:
    using ISubscr = ISubscription<void, Args...>;
    using ISubscrPtr = std::unique_ptr<ISubscr>;

    struct ObserversContainer {
        std::mutex lock;
        std::vector<ISubscrPtr> observers;
    };

    struct HandlerImpl : public IObserverConnection {
        std::weak_ptr<ObserversContainer> observers;
        ISubscr* thisObserver = nullptr;
        bool releaseInDestructor = false;

        explicit HandlerImpl(std::weak_ptr<ObserversContainer> observers, ISubscr* thisObserver, bool release)
            : observers(observers)
            , thisObserver(thisObserver)
            , releaseInDestructor(release)
        {
        }

        IObserverConnectionHandler makeScopedDestructor() override
        {
            this->releaseInDestructor = false;
            auto handler = std::make_unique<HandlerImpl>(*this);
            handler->releaseInDestructor = true;
            return handler;
        }

        virtual ~HandlerImpl()
        {
            if(releaseInDestructor) {
                auto o = observers.lock();

                if(o) {
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

    IObserverConnectionHandler subscribe(std::unique_ptr<ISubscription<void, Args...>> observer) override
    {
        auto handler = std::make_unique<HandlerImpl>(mObservers, observer.get(), false);
        std::lock_guard<std::mutex> lock(mObservers->lock);
        mObservers->observers.push_back(std::move(observer));
        return handler;
    }

    void unsubscribe(std::unique_ptr<ISubscription<void, Args...>> observer) override
    {
        unsubscr(this->mObservers, [&observer](const ISubscrPtr& uPtr) { return uPtr->isSame(*observer); });
    }

    void unsubscribe() override
    {
        std::lock_guard<std::mutex> lock(mObservers->lock);
        mObservers->observers.clear();
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

    std::size_t subsribersCount() const
    {
        return mObservers->observers.size();
    }

private:
    template <typename Predicate>
    static void unsubscr(std::shared_ptr<ObserversContainer> observers, Predicate predicate)
    {
        std::lock_guard<std::mutex> lock(observers->lock);
        observers->observers.erase(
            std::remove_if(observers->observers.begin(),
                observers->observers.end(),
                predicate),
            observers->observers.end());
    }

    std::shared_ptr<ObserversContainer> mObservers;
};

template <typename... Args>
class EventSimpleSt : public IEventSimpleSt<Args...> {
public:
    using ISubscrPtr = std::unique_ptr<typename IEventSimpleSt<Args...>::ISubscr>;
    using SubsrHandller = typename IEventSimpleSt<Args...>::ISubscr*;

    struct ObserversContainer {
        std::vector<ISubscrPtr> observers;
        std::vector<SubsrHandller> deleteAfter;
    };

    virtual ~EventSimpleSt() = default;
    EventSimpleSt()
    {
        mObservers = std::make_shared<ObserversContainer>();
    }

    SubsrHandller subscribe(std::unique_ptr<ISubscription<void, Args...>> observer) override
    {
        auto rv = observer.get();
        mObservers->observers.push_back(std::move(observer));
        return rv;
    }

    void unsubscribe(std::unique_ptr<ISubscription<void, Args...>> observer) override
    {
        if(notifying){
            auto item = std::find_if(std::cbegin(mObservers->observers), std::cend(mObservers->observers), [&observer](const auto& o){return observer->isSame(*o);});
            mObservers->deleteAfter.push_back(item->get());
        }
        else{
            unsubscr(this->mObservers, [&observer](const ISubscrPtr& uPtr) { return uPtr->isSame(*observer); });
        }
    }

    void unsubscribe(SubsrHandller hdl)
    {
        if(notifying){
            mObservers->deleteAfter.push_back(hdl);
        }
        else{
            unsubscr(this->mObservers, [hdl](const ISubscrPtr& uPtr) { return uPtr.get() == hdl; });
        }
    }

    void unsubscribe() override
    {
        mObservers->observers.clear();
        mObservers->deleteAfter.clear();
    }

    void notify(Args... args)
    {
        notifying = true;
        for(auto& obs : mObservers->observers)
            obs->raise(args...);
        notifying = false;
        for(auto obs : mObservers->deleteAfter)
            unsubscribe(obs);
        mObservers->deleteAfter.clear();
    }

    std::size_t subsribersCount() const
    {
        return mObservers->observers.size();
    }

private:
    bool notifying = false;
    template <typename Predicate>
    static void unsubscr(std::shared_ptr<ObserversContainer> observers, Predicate predicate)
    {
        observers->observers.erase(
            std::remove_if(observers->observers.begin(),
                           observers->observers.end(),
                           predicate),
            observers->observers.end());
    }

    std::shared_ptr<ObserversContainer> mObservers;
};

}
