#include "AppContainer.h"

#include "Elektromer.h"
#include "Log.h"
#include "Pru.h"
#include "TxtEnum.h"
#include "bytd/src/therm.h"
#include "git_revision_description.h"

std::string AppContainer::doRequest(std::string msg)
{
    LogINFO("AppContainer::doRequest {}", msg);
    std::string rsp = "unknown_cmd";
    std::istringstream ss(msg);
    std::string cmd;
    ss >> cmd;
    if(cmd == "otTransfer") {
        std::string sreq;
        ss >> sreq;
        openTherm->asyncTransferRequest(std::stoi(sreq, nullptr, 0));
        rsp = "ok_async";
    } else if(cmd == "setLogLevel") {
        std::string level;
        ss >> level;
        Util::Log::instance().get()->set_level(spdlog::level::from_str(level));
        rsp = "ok";
    } else if(cmd == "rev") {
        rsp = GIT_REV_DESC_STR;
    }
    else if(cmd == "setNewTotal")
    {
        std::string dev;
        float val;
        ss >> dev >> val;
        if(dev == "elektromer"){
            components.elektromer->setNewValue(val);
            rsp = "ok";
        }
        else if(dev == "vodomer"){
            components.vodomer->setNewValue(val);
            rsp = "ok";
        }
        else{
            rsp = "setNewTotal error";
        }
    }
    return rsp;
}

AppContainer::AppContainer()
    : signals(io_service, SIGINT, SIGTERM)
    , t1sec(io_service, std::chrono::seconds(1))
    , timerKurenie(io_service, kurenie::Kurenie::dT)
    , canBus(io_service)
{
    LogINFO("revison {}", GIT_REV_DESC_STR);
    signals.async_wait([this](auto error, auto signum) {
        LogINFO("signal {} {}", signum, error.message());
        io_service.stop();
    });

    mqtt = std::make_shared<MqttClient>(io_service);
    // exporter = std::make_shared<ow::Exporter>();

    auto can_init_sent_ok = canBus.send(can::mkMsgSetAllStageOperating().frame);
    if(not can_init_sent_ok) {
        LogERR("can init sent failed");
    }
}

AppContainer::~AppContainer() { }

struct Meranie {
    MeranieTeploty& meranie;
    std::atomic_bool running;
    std::thread meas_thread;
    explicit Meranie(MeranieTeploty& meranie)
        : meranie(meranie)
        , running(true)
    {
        meas_thread = std::thread([this] {
            while(running.load()) {
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

bool matchTopicBase(const std::string& topic, const char* base)
{
    return topic.substr(0, std::char_traits<char>::length(base)) == base;
}

float safeConvertF(const std::string& topic, const std::string& msg)
{
    auto v = std::numeric_limits<float>::quiet_NaN();
    try {
        v = std::stof(msg);
    } catch(std::exception& e) {
        LogERR("conversion error {} {}", topic, msg);
    }
    return v;
}

void AppContainer::run()
{
    slovpwm = std::make_unique<slowswi2cpwm>();
    auto pru = std::make_shared<Pru>();
    openTherm = std::make_shared<OpenTherm>(pru, mqtt);
    auto builder = std::make_unique<Builder>(mqtt);
    auto tsensors = builder->buildBBoW();
    auto canInputControl = builder->buildCan(canBus);
    builder->buildMisc(*slovpwm, *openTherm);
    builder->vypinace(io_service);
    components = builder->getComponents();
    builder = nullptr;
    auto meranie = std::make_unique<MeranieTeploty>(pru, std::move(tsensors), *mqtt);

    components.elektromer = std::make_unique<Elektromer>(*mqtt);
    components.vodomer = std::make_unique<Vodomer>(*mqtt);

    openTherm->Flame = [this](bool flameOn) {
        if(!flameOn) {
            components.pumpa->onPlamenOff();
        }
    };

    mqtt->OnMsgRecv = [this](auto topic, auto msg) {
        if(not matchTopicBase(topic, mqtt::rootTopic)) {
            return;
        }

        LogINFO("OnMsgRecv {}:{}", topic, msg);

        if(matchTopicBase(topic, mqtt::devicesTopic)) {
            const auto name = topic.substr(topic.find_last_of('/') + 1);
            auto devIt = components.devicesOnOff.find(name);
            auto v = std::stod(msg);
            if(devIt != std::cend(components.devicesOnOff)) {
                devIt->second->set(v, true);
                return;
            }
            if(v) {
                if(components.brana->getName() == name) {
                    components.brana->trigger();
                    return;
                }
                if(components.dverePavlac->getName() == name) {
                    components.dverePavlac->trigger();
                    return;
                }
            }
        } else if(matchTopicBase(topic, mqtt::kurenieSetpointTopic)) {
            const auto name = topic.substr(topic.find_last_of('/') + 1);
            auto room = kurenie::txtToRoom(name);
            if(room != kurenie::ROOM::_last) {
                auto v = safeConvertF(topic, msg);
                components.kurenie->setSP(room, v);
            }
        } else if(matchTopicBase(topic, mqtt::tevOverrideTopic)) {
            const auto name = topic.substr(topic.find_last_of('/') + 1);
            auto room = kurenie::txtToRoom(name);
            if(room != kurenie::ROOM::_last) {
                auto v = safeConvertF(topic, msg);
                components.kurenie->override_pwm_TEV(room, v);
            }
        } else if(topic == "rb/ctrl/override/setpointCH") {
            components.kurenie->override_t_CH(safeConvertF(topic, msg));
        } else if(topic == "rb/ctrl/ot/setpoint/dhw") {
            openTherm->dhwSetpoint = safeConvertF(topic, msg);
        } else if(topic == "rb/ctrl/pumpa") {
            auto v = std::stod(msg);
            if(v) {
                components.pumpa->start();
            } else {
                components.pumpa->stop();
            }
        } else if(topic == mqtt::ventil4w_target) {
            try {
                if(components.ventil) {
                    components.ventil->moveTo(msg);
                } else {
                    LogERR("components.ventil nullptr");
                }
            } catch(std::exception& e) {
                LogERR("components.ventil {}", e.what());
            } catch(...) {
                LogERR("components.ventil unknown exception");
            }
        } else if(topic == "rb/ctrl/req") {
            try {
                auto rsp = doRequest(msg);
                mqtt->publish(mqtt::rbResponse, rsp, false, 2);
            } catch(std::exception& e) {
                LogERR("request exception: {}", e.what());
                mqtt->publish(mqtt::rbResponse, e.what(), false, 2);
            } catch(...) {
                LogERR("request unkown exception");
                mqtt->publish(mqtt::rbResponse, "unkown exception", false, 2);
            }
        }
    };

    sched_1sect();
    sched_kurenie();

    Meranie meranie_active_object(*meranie);
    ::sd_notify(0, "READY=1");
    io_service.run();
    LogINFO("io service exited");
    components.elektromer->store();
    components.vodomer->store();
}

void AppContainer::on1sec()
{
    mqtt->do_misc();
}

void AppContainer::sched_1sect()
{
    t1sec.async_wait([this](const boost::system::error_code&) {
        on1sec();
        t1sec.expires_at(t1sec.expiry() + std::chrono::seconds(1));
        sched_1sect();
    });
}

void AppContainer::sched_kurenie()
{
    timerKurenie.async_wait([this](const boost::system::error_code&) {
        components.kurenie->process(kurenie::Clock::now());
        timerKurenie.expires_at(timerKurenie.expiry() + kurenie::Kurenie::dT);
        sched_kurenie();
    });
}
