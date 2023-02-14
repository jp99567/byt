
#include <systemd/sd-daemon.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <gpiod.hpp>

#include "Log.h"
#include "thread_util.h"
#include "exporter.h"
#include "therm.h"
#include "Elektromer.h"
#include "Pru.h"
#include "OpenTherm.h"
#include <boost/asio.hpp>
#include "mqtt.h"
#include "slowswi2cpwm.h"
#include "builder.h"
#include "IIo.h"

class BBDigiOut : public IDigiOut
{
    gpiod::line line;
public:
    explicit BBDigiOut(gpiod::chip& chip, unsigned lineNr)
        :line(chip.get_line(lineNr))
    {
        gpiod::line_request req;
        req.request_type = gpiod::line_request::DIRECTION_OUTPUT;
        req.consumer = "bytd";
        line.request(req);
        if( line.direction() != gpiod::line::DIRECTION_OUTPUT )
        {
            LogERR("BBDigiOut direction not output");
        }
    }

    operator bool() const override
    {
        return line.get_value();
    }

    bool operator()(bool value) override
    {
        line.set_value(value);
        return value;
    }

    ~BBDigiOut(){}
};

class Pumpa
{
    constexpr static int maxPlamenOff = 6;
    std::unique_ptr<IDigiOut> out;
    int plamenOffCount = -1;
    std::shared_ptr<MqttClient> mqtt;

public:
    explicit Pumpa(std::unique_ptr<IDigiOut> digiout, std::shared_ptr<MqttClient> mqtt)
        :out(std::move(digiout))
        ,mqtt(mqtt)
    {
        (*out)(false);
    }

    void start()
    {
        LogINFO("Pumpa start {}", plamenOffCount);
        setPumpa(true);
        plamenOffCount = 0;
    }

    void stop()
    {
        LogINFO("Pumpa stop {}", plamenOffCount);
        setPumpa(false);
        plamenOffCount = -1;
    }

    void onPlamenOff()
    {
        if(plamenOffCount >= 0){
            LogINFO("Pumpa onPlamenOff {}", plamenOffCount);
            if(++plamenOffCount > maxPlamenOff){
                stop();
                LogINFO("pumpa automatic Off");
            }
        }
    }

    ~Pumpa()
    {
        (*out)(false);
    }

private:
    void setPumpa(bool value)
    {
        if( value != *out)
        {
            (*out)(value);
            mqtt->publish("rb/stat/pumpa", (int)value);
        }
    }
};

class AppContainer
{
	boost::asio::io_service io_service;
    boost::asio::signal_set signals;
    boost::asio::steady_timer t1sec;
	ow::ExporterSptr exporter;
	std::shared_ptr<MeranieTeploty> meranie;
	std::shared_ptr<OpenTherm> openTherm;
    std::shared_ptr<MqttClient> mqtt;
    std::unique_ptr<slowswi2cpwm> slovpwm;
    std::shared_ptr<gpiod::chip> gpiochip3;
    std::unique_ptr<Pumpa> pumpa;

public:
	AppContainer()
        :signals(io_service, SIGINT, SIGTERM)
        ,t1sec(io_service, std::chrono::seconds(1))
    {
		signals.async_wait([this](auto error, auto signum){
			LogINFO("signal {}",signum);
            io_service.stop();
		});

        //mqtt = std::make_shared<MqttClient>(io_service);
        //exporter = std::make_shared<ow::Exporter>();
    }

    void on1sec()
    {
        mqtt->do_misc();
    }

    void sched_1sect()
    {
        t1sec.async_wait([this](const boost::system::error_code&){
            on1sec();
            t1sec.expires_at(t1sec.expiry() + std::chrono::seconds(1));
            sched_1sect();
        });
    }

	void run()
	{
		bool running = true;
		std::mutex cvlock;
		std::condition_variable cv_running;

        Builder builder;
        auto tsensors = builder.buildBBoW();
        builder.buildCan();
        auto pru = std::make_shared<Pru>();
        openTherm = std::make_shared<OpenTherm>(pru, *mqtt);
        meranie = std::make_shared<MeranieTeploty>(pru, std::move(tsensors), *mqtt);
        slovpwm = std::make_unique<slowswi2cpwm>();
        Elektromer elektomer(*mqtt);

        gpiochip3 = std::make_shared<gpiod::chip>("3");
        pumpa = std::make_unique<Pumpa>(std::make_unique<BBDigiOut>(*gpiochip3, 21), mqtt);

        openTherm->Flame = [this](bool flameOn){
            if(!flameOn){
                pumpa->onPlamenOff();
            }
        };

		std::thread meastempthread([this,&running,&cv_running,&cvlock]{
			thread_util::set_thread_name("bytd-meranie");

			do{
                std::this_thread::sleep_for(std::chrono::seconds(1));
				{
                    const auto path = std::filesystem::path("/run/bytd/setpoint_dhw");
                    if(std::filesystem::exists(path)){
                        std::ifstream f(path);
                        f >> openTherm->dhwSetpoint;
                    }
				}
				{
                    const auto path = std::filesystem::path("/run/bytd/setpoint_ch");
                    if(std::filesystem::exists(path)){
                        std::ifstream f(path);
                        f >> openTherm->chSetpoint;
                    }
				}
                meranie->meas();
			}
			while(running);
		});

        mqtt->OnMsgRecv = [this](auto topic, auto msg){
            if(topic.substr(0,8) != "rb/ctrl/")
                return;

            LogINFO("OnMsgRecv {}:{}", topic, msg);
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

        ::sd_notify(0, "READY=1");
        io_service.run();
        LogINFO("io service exited");

		{
			std::unique_lock<std::mutex> guard(cvlock);
			running = false;
			cv_running.notify_all();
		}
		meastempthread.join();
	}

    void doRequest(std::string msg)
    {
        std::istringstream ss(msg);
        std::string cmd;
        ss >> cmd;
        if(cmd == "otTransfer"){
            std::string sreq;
            ss >> sreq;
            openTherm->asyncTransferRequest(std::stoi(sreq, nullptr,0));
        }
    }
};

int main()
{
  Util::LogExit scopedLog("GRACEFUL");
  MqttWrapper libmMosquitto;

  try
  {
      AppContainer().run();
  }
  catch (std::exception& e)
  {
      LogERR("app crash: {}", e.what());
      return 1;
  }
  catch (...)
  {
       LogERR("app crash: unknown");
       return 1;
  }

  return 0;
}
