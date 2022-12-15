
#include <systemd/sd-daemon.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>

#include "Log.h"
#include "thread_util.h"
#include "exporter.h"
#include "therm.h"
#include "Elektromer.h"
#include "Pru.h"
#include "OpenTherm.h"
#include "ICore.h"
#include <boost/asio.hpp>
#include "mqtt.h"
#include "slowswi2cpwm.h"

#include <yaml-cpp/yaml.h>

class Facade : public ICore
{
};

void dumpYaml(YAML::Node& node, int level=0);

void dumpYaml(YAML::Node& node, int level)
{
    std::string offset(1+2*level, '-');
    offset.append("YAML");
    switch (node.Type()) {
    case YAML::NodeType::value::Scalar:
        LogINFO("{} Scalar: {}", offset, node.as<std::string>());
        break;
    case YAML::NodeType::value::Sequence:
        LogINFO("{} seq{}:", offset, node.size());
        for(auto it = node.begin(); it != node.end(); ++it) {
          auto element = *it;
          dumpYaml(element, level+1);
        }
        break;
    case YAML::NodeType::value::Map:
        LogINFO("{} map:", offset);
        for(auto it = node.begin(); it != node.end(); ++it) {
          LogINFO("{} map iterate {} {}:", offset, (int)(it->first.Type()), (int)(it->second.Type()) );
          dumpYaml(it->first, level+1);
          dumpYaml(it->second, level+1);
        }
        break;
    default:
        LogINFO("YAML null or undefined");
        break;
    }
}

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

public:
	AppContainer()
        :signals(io_service, SIGINT, SIGTERM)
        ,t1sec(io_service, std::chrono::seconds(1))
    {
		signals.async_wait([this](auto error, auto signum){
			LogINFO("signal {}",signum);
            io_service.stop();
		});

        mqtt = std::make_shared<MqttClient>(io_service);
        exporter = std::make_shared<ow::Exporter>();
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

        YAML::Node config = YAML::LoadFile("config.yaml");
        dumpYaml(config);

        auto node = config["BBOw"];
        std::vector<std::tuple<std::string, std::string>> tsensors;
        for(auto it = node.begin(); it != node.end(); ++it){
            auto name = it->first.as<std::string>();
            auto romcode = (it->second)["owRomCode"].as<std::string>();
            LogINFO("yaml configured sensor {} {}", name, "a");
            tsensors.emplace_back(std::make_tuple(name, romcode));
        }

        auto pru = std::make_shared<Pru>();
        openTherm = std::make_shared<OpenTherm>(pru, *mqtt);
        meranie = std::make_shared<MeranieTeploty>(pru, std::move(tsensors), *mqtt);
        slovpwm = std::make_unique<slowswi2cpwm>();

		std::thread meastempthread([this,&running,&cv_running,&cvlock]{
			thread_util::set_thread_name("bytd-meranie");

            //Elektromer em;
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
            LogINFO("mqtt msg {}:{}", topic, msg);
            if(topic == "rb/ot/setpoint/ch"){
                auto v = std::stof(msg);
                openTherm->chSetpoint = v;
            }
            else if(topic == "rb/ot/setpoint/dhw"){
                auto v = std::stof(msg);
                openTherm->dhwSetpoint = v;
            }
            else if(topic == "rb/i2cpwm/kuchyna"){
                auto v = std::stof(msg);
                slovpwm->update(0,v);
            }
            else if(topic == "rb/i2cpwm/obyvka"){
                auto v = std::stof(msg);
                slovpwm->update(1,v);
            }
            else if(topic == "rb/i2cpwm/spalna"){
                auto v = std::stof(msg);
                slovpwm->update(2,v);
            }
            else if(topic == "rb/i2cpwm/kupelna"){
                auto v = std::stof(msg);
                slovpwm->update(3,v);
            }
            else if(topic == "rb/i2cpwm/podlaha"){
                auto v = std::stof(msg);
                slovpwm->update(4,v);
            }
            else if(topic == "rb/i2cpwm/izba"){
                auto v = std::stof(msg);
                slovpwm->update(5,v);
            }
            else if(topic == "rb/i2cpwm/vetranie"){
                auto v = std::stod(msg);
                slovpwm->dig1Out(v);
            }
            else if(topic == "rb/i2cpwm/dvere"){
                auto v = std::stod(msg);
                slovpwm->dig2Out(v);
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
