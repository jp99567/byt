#include "AppContainer.h"

#include "BBDigiOut.h"
#include "Log.h"
#include "Elektromer.h"
#include "Pru.h"
#include "builder.h"
#include "bytd/src/therm.h"

void AppContainer::doRequest(std::string msg)
{
    LogINFO("AppContainer::doRequest {}", msg);
    std::istringstream ss(msg);
    std::string cmd;
    ss >> cmd;
    if(cmd == "otTransfer"){
        std::string sreq;
        ss >> sreq;
        openTherm->asyncTransferRequest(std::stoi(sreq, nullptr,0));
    }
    else if(cmd == "setLogLevel"){
        std::string level;
        ss >> level;
        Util::Log::instance().get()->set_level(spdlog::level::from_str(level));
    }
}

AppContainer::AppContainer()
    :signals(io_service, SIGINT, SIGTERM)
    ,t1sec(io_service, std::chrono::seconds(1))
    ,canBus(io_service)
{
    signals.async_wait([this](auto error, auto signum){
        LogINFO("signal {}",signum);
        io_service.stop();
    });

    mqtt = std::make_shared<MqttClient>(io_service);
    //exporter = std::make_shared<ow::Exporter>();
}

struct Meranie
{
    MeranieTeploty& meranie;
    std::atomic_bool running;
    std::thread meas_thread;
    explicit Meranie(MeranieTeploty& meranie) : meranie(meranie), running(true)
    {
        meas_thread = std::thread([this]{
            while(running.load()){
                    auto t0 = std::chrono::steady_clock::now();
                    this->meranie.meas();
                    std::this_thread::sleep_until(t0 + std::chrono::seconds(1));
            }
        });
    }
    ~Meranie()
    {
        running.store(false);
        meas_thread.join();
    }
};

void AppContainer::run()
{
    slovpwm = std::make_unique<slowswi2cpwm>();
    auto pru = std::make_shared<Pru>();
    auto builder = std::make_unique<Builder>(mqtt);
    auto tsensors = builder->buildBBoW();
    auto canInputControl = builder->buildCan(canBus);
    builder->buildMisc(*slovpwm);
    builder->vypinace(io_service);
    auto components = builder->getComponents();
    builder = nullptr;
    auto meranie = std::make_unique<MeranieTeploty>(pru, std::move(tsensors), *mqtt);

    openTherm = std::make_shared<OpenTherm>(pru, mqtt);
    Elektromer elektomer(*mqtt);

    gpiochip3 = std::make_shared<gpiod::chip>("3");
    pumpa = std::make_unique<Pumpa>(std::make_unique<BBDigiOut>(*gpiochip3, 21), mqtt);

    openTherm->Flame = [this](bool flameOn){
        if(!flameOn){
            pumpa->onPlamenOff();
        }
    };

    mqtt->OnMsgRecv = [this, &components](auto topic, auto msg){
        if(topic.substr(0,std::char_traits<char>::length(mqtt::rootTopic)) != mqtt::rootTopic)
            return;

        LogINFO("OnMsgRecv {}:{}", topic, msg);

        if(topic.substr(0,std::char_traits<char>::length(mqtt::devicesTopic)) == mqtt::devicesTopic){
            const auto name = topic.substr(topic.find_last_of('/') + 1);
            auto devIt = components.devicesOnOff.find(name);
            auto v = std::stod(msg);
            if(devIt != std::cend(components.devicesOnOff)){
                devIt->second->set(v, true);
                return;
            }
            if(v){
                if(components.brana->getName() == name){
                    components.brana->trigger();
                    return;
                }
                if(components.dverePavlac->getName() == name){
                    components.dverePavlac->trigger();
                    return;
                }
            }
        }


        if(topic == "rb/ctrl/ot/setpoint/ch"){
            auto v = std::stof(msg);
            openTherm->chSetpoint = v;
        }
        else if(topic == "rb/ctrl/ot/setpoint/dhw"){
            auto v = std::stof(msg);
            openTherm->dhwSetpoint = v;
        }
        else if(topic == "rb/ctrl/i2cpwm/kuchyna"){
            auto v = std::stof(msg);
            slovpwm->update(0,v);
        }
        else if(topic == "rb/ctrl/i2cpwm/obyvka"){
            auto v = std::stof(msg);
            slovpwm->update(1,v);
        }
        else if(topic == "rb/ctrl/i2cpwm/spalna"){
            auto v = std::stof(msg);
            slovpwm->update(2,v);
        }
        else if(topic == "rb/ctrl/i2cpwm/kupelna"){
            auto v = std::stof(msg);
            slovpwm->update(3,v);
        }
        else if(topic == "rb/ctrl/i2cpwm/podlaha"){
            auto v = std::stof(msg);
            slovpwm->update(4,v);
        }
        else if(topic == "rb/ctrl/i2cpwm/izba"){
            auto v = std::stof(msg);
            slovpwm->update(5,v);
        }
        else if(topic == "rb/ctrl/pumpa"){
            auto v = std::stod(msg);
            if(v){
                pumpa->start();
            }
            else {
                pumpa->stop();
            }
        }
        else if(topic == "rb/ctrl/req"){
            try{
                doRequest(msg);
            }
            catch (std::exception& e)
            {
                LogERR("request exception: {}", e.what());
                mqtt->publish(mqtt::rbResponse, e.what(), false);
            }
            catch (...)
            {
                LogERR("request unkown exception");
                mqtt->publish(mqtt::rbResponse, "unkown exception", false);
            }

        }
    };

    sched_1sect();


    canBus.send(can::mkMsgSetAllStageOperating().frame);
    Meranie meranie_active_object(*meranie);
    ::sd_notify(0, "READY=1");
    io_service.run();
    LogINFO("io service exited");
}

void AppContainer::on1sec()
{
    mqtt->do_misc();
}

void AppContainer::sched_1sect()
{
    t1sec.async_wait([this](const boost::system::error_code&){
        on1sec();
        t1sec.expires_at(t1sec.expiry() + std::chrono::seconds(1));
        sched_1sect();
    });
}
