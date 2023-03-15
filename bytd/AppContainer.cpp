#include "AppContainer.h"

#include "BBDigiOut.h"
#include "Log.h"
#include "thread_util.h"
#include "Elektromer.h"
#include "Pru.h"
#include "builder.h"

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

void AppContainer::run()
{
    auto pru = std::make_shared<Pru>();
    std::unique_ptr<can::InputControl> canInputControl;
    auto builder = std::make_unique<Builder>(mqtt);
    auto tsensors = builder->buildBBoW();
    canInputControl = builder->buildCan(canBus);
    auto devicesOnOff = builder->buildOnOffDevices();
    builder = nullptr;
    auto meranie = std::make_unique<MeranieTeploty>(pru, std::move(tsensors), *mqtt);

    openTherm = std::make_shared<OpenTherm>(pru, *mqtt);
    slovpwm = std::make_unique<slowswi2cpwm>();
    Elektromer elektomer(*mqtt);

    gpiochip3 = std::make_shared<gpiod::chip>("3");
    pumpa = std::make_unique<Pumpa>(std::make_unique<BBDigiOut>(*gpiochip3, 21), mqtt);

    openTherm->Flame = [this](bool flameOn){
        if(!flameOn){
            pumpa->onPlamenOff();
        }
    };

    mqtt->OnMsgRecv = [this, &devicesOnOff](auto topic, auto msg){
        if(topic.substr(0,8) != "rb/ctrl/")
            return;

        LogINFO("OnMsgRecv {}:{}", topic, msg);

        auto devIt = devicesOnOff.find(topic);
        if(devIt != std::cend(devicesOnOff)){
            auto v = std::stod(msg);
            devIt->second->set(v, true);
        }
        else if(topic == "rb/ctrl/ot/setpoint/ch"){
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
        else if(topic == "rb/ctrl/i2cpwm/vetranie"){
            auto v = std::stod(msg);
            slovpwm->dig1Out(v);
        }
        else if(topic == "rb/ctrl/i2cpwm/dvere"){
            auto v = std::stod(msg);
            slovpwm->dig2Out(v);
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

    std::atomic_bool running = true;
    std::thread meas_thread([&meranie, &running]{
        while(running.load()){
                auto t0 = std::chrono::steady_clock::now();
                meranie->meas();
                std::this_thread::sleep_until(t0 + std::chrono::seconds(1));
        }
    });
    canBus.send(can::mkMsgSetAllStageOperating().frame);
    ::sd_notify(0, "READY=1");
    io_service.run();
    LogINFO("io service exited");
    running.store(true);
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