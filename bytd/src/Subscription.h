#pragma once

#include <functional>
#include <memory>

namespace event {

template<typename TResult = void, typename... TArgs>
class ISubscription
{
public:
    virtual ~ISubscription() { }
    
    // notifies observers about event occurence
    virtual TResult raise(TArgs...) = 0;
    
    // compares if delagates reference same target method & listener
    virtual bool isSame(const ISubscription&) const = 0;
    
    // Returns a deep copy of itself
    virtual std::unique_ptr<ISubscription> clone() const = 0;
};

template<class TResult = void, typename... TArgs>
class FSubscription : public ISubscription<TResult, TArgs...>
{
public:
    typedef TResult (*Target)(TArgs...);
    
    FSubscription(Target target) : mTarget(target) { }
    
    TResult raise(TArgs... args) { return (*mTarget)(args...); }
    
    bool isSame(const ISubscription<TResult, TArgs...>& del) const
    {
        const FSubscription* other = reinterpret_cast<const FSubscription*>(&del);
        return other && mTarget == other->mTarget;
    }
    
    std::unique_ptr<ISubscription<TResult, TArgs...>> clone() const
    {
        return std::make_unique<FSubscription<TResult, TArgs...>>(mTarget);
    }
private:
    Target mTarget;
};

template<class TResult = void, typename... TArgs>
class LSubscription : public ISubscription<TResult, TArgs...>
{
public:
    using Target = std::function<TResult(TArgs...)>;
    
    LSubscription(const Target& target) : mTarget(target) { }
    
    TResult raise(TArgs... args) { return mTarget(args...); }
    
    bool isSame(const ISubscription<TResult, TArgs...>&) const
    {
        return false; // C++ lambdas are simply bad.
    }
    
    std::unique_ptr<ISubscription<TResult, TArgs...>> clone() const
    {
        return std::make_unique<LSubscription<TResult, TArgs...>>(mTarget);
    }
private:
    Target mTarget;
};

namespace maXYmo_priv
{

template <bool CallbackConstness, class TListener, class TResult, typename...TArgs> struct CallbackType
{ };

template < class TListener, class TResult, typename...TArgs> struct CallbackType<true, TListener, TResult, TArgs...>
{
    typedef TResult (TListener::*Target)(TArgs...) const;
};

template < class TListener, class TResult, typename...TArgs> struct CallbackType<false, TListener, TResult, TArgs...>
{
    typedef TResult (TListener::*Target)(TArgs...);
};

}

template<class TListener, class TResult, bool CallbackConstness, typename... TArgs>
class Subscription : public ISubscription<TResult, TArgs...>
{
public:
    using Target = typename maXYmo_priv::CallbackType<CallbackConstness, TListener, TResult, TArgs...>::Target;
    
    Subscription(TListener *listener, Target target) 
        : mListener(listener), mTarget(target) {}
    
    TResult raise(TArgs... args)
    {
        return (mListener->*mTarget)(args...);
    }
    
    bool isSame(const ISubscription<TResult, TArgs...>& del) const
    {
        const Subscription* other = reinterpret_cast<const Subscription*>(&del);
        return other && mListener == other->mListener && mTarget == other->mTarget;
    }

    std::unique_ptr<ISubscription<TResult, TArgs...>> clone() const
    {
        return std::make_unique<Subscription<TListener,TResult, CallbackConstness, TArgs...>>(mListener, mTarget);
    }
    
private:
    TListener* mListener;
    Target mTarget;
};

template<class TListener, class TResult = void, typename... TArgs>
std::unique_ptr<ISubscription<TResult, TArgs...>> subscr(TListener *listener,  TResult (TListener::*target)(TArgs...) const)
{
    return std::make_unique<Subscription<TListener, TResult, true, TArgs...>>(listener, target);
}

template<class TListener, class TResult = void, typename... TArgs>
std::unique_ptr<ISubscription<TResult, TArgs...>> subscr(TListener *listener,  TResult (TListener::*target)(TArgs...))
{
    return std::make_unique<Subscription<TListener, TResult, false, TArgs...>>(listener, target);
}

template <class TResult = void, typename... TArgs>
std::unique_ptr<ISubscription<TResult, TArgs...>> subscr(TResult (*target)(TArgs...))
{
    return std::make_unique<FSubscription<TResult, TArgs...>>(target);
}

template<typename Lambda>
struct lambda_traits
 : public lambda_traits<decltype(&Lambda::operator())>
{};

template <typename ClassType, typename ReturnType, typename... Args>
struct lambda_traits<ReturnType(ClassType::*)(Args...) const>
{
    using stdFunc = typename LSubscription<ReturnType, Args...>::Target;
};

template<typename Lambda>
typename lambda_traits<Lambda>::stdFunc
lambda_to_subscr(const Lambda& lambda)
{
    return static_cast<typename lambda_traits<Lambda>::stdFunc>(lambda);
}

namespace maXYmo_priv
{

template <class TResult = void, typename... TArgs>
std::unique_ptr<ISubscription<TResult, TArgs...>> lambda_subscr_impl(const std::function<TResult(TArgs...)>& lambda)
{
    return std::make_unique<LSubscription<TResult, TArgs...>>(lambda);
}

}

template <class Lambda, typename std::enable_if<!std::is_pointer<Lambda>::value || (!std::is_function<typename std::remove_pointer<Lambda>::type>::value)>::type* dummy = nullptr>
decltype(auto) subscr(const Lambda& lambda)
{
    return maXYmo_priv::lambda_subscr_impl(lambda_to_subscr(lambda));
}

}
