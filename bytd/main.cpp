
#include <systemd/sd-daemon.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>

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

#include "yaml-cpp/yaml.h"
#include <boost/asio/ip/tcp.hpp>

class Facade : public ICore
{
};

class AppContainer
{
	boost::asio::io_service io_service;
    boost::asio::signal_set signals;
	ow::ExporterSptr exporter;
	std::shared_ptr<MeranieTeploty> meranie;
	std::shared_ptr<OpenTherm> openTherm;
    std::shared_ptr<MqttClient> mqtt;

    std::unique_ptr<boost::asio::ip::tcp::socket> mqtt_socket;

public:
	AppContainer()
	:signals(io_service, SIGINT, SIGTERM)
    {
		signals.async_wait([this](auto error, auto signum){
			LogINFO("signal {}",signum);
            io_service.stop();
		});

        //exporter = std::make_shared<ow::Exporter>();
        //auto pru = std::make_shared<Pru>();
        //meranie = std::make_shared<MeranieTeploty>(pru, exporter);
        //openTherm = std::make_shared<OpenTherm>(pru);
        mqtt = std::make_shared<MqttClient>();
    }

    void sched_read_mqtt_socket()
    {
        if(mqtt_socket){
            mqtt_socket->async_wait(boost::asio::socket_base::wait_read, [this](const boost::system::error_code& error){
                if(!error){
                    mqtt->do_read();
                    sched_read_mqtt_socket();
                    sched_write_mqtt_socket();
                }
            });
        }
    }

    void sched_write_mqtt_socket()
    {
        if(mqtt->is_write_ready()){
            mqtt_socket->async_wait(boost::asio::socket_base::wait_write, [this](const boost::system::error_code& error){
                if(!error){
                    mqtt->do_write();
                    sched_write_mqtt_socket();
                }
            });
        }
    }

	void run()
	{
		bool running = true;
		std::mutex cvlock;
		std::condition_variable cv_running;

		std::thread meastempthread([this,&running,&cv_running,&cvlock]{
			thread_util::set_thread_name("bytd-meranie");

            //Elektromer em;
			do{
                std::this_thread::sleep_for(std::chrono::seconds(1));
/*				meranie->meas();
				std::unique_lock<std::mutex> ulck(cvlock);
				cv_running.wait_for(ulck, std::chrono::seconds(10), [&running]{
					return !running;
				});

				{
					std::ifstream fdhw("/run/bytd/setpoint_dhw");
					fdhw >> openTherm->dhwSetpoint;
				}
				{
					std::ifstream fch("/run/bytd/setpoint_ch");
					fch >> openTherm->chSetpoint;
				}
*/
			}
			while(running);
		});

        auto sfd = mqtt->socket();
        mqtt_socket = std::make_unique<boost::asio::ip::tcp::socket>(io_service, boost::asio::ip::tcp::v4(), sfd);
        sched_read_mqtt_socket();
        sched_write_mqtt_socket();

		sd_notify(0, "READY=1");

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

void dumpYaml(YAML::Node& node);

void dumpYaml(YAML::Node& node)
{
    switch (node.Type()) {
    case YAML::NodeType::value::Scalar:
        LogINFO("YANL Scalar: {}", node.as<std::string>());
        break;
    case YAML::NodeType::value::Sequence:
        LogINFO("YAML seq{}:", node.size());
        for(auto it = node.begin(); it != node.end(); ++it) {
          auto element = *it;
          dumpYaml(element);
        }
        break;
    case YAML::NodeType::value::Map:
        LogINFO("YAML map:");
        for(auto it = node.begin(); it != node.end(); ++it) {
          LogINFO("YAML map iterate {} {}:", (int)(it->first.Type()), (int)(it->second.Type()) );
          dumpYaml(it->first);
          dumpYaml(it->second);
        }
        break;
    default:
        LogINFO("YANL null or undefined");
        break;
    }
}

int main()
{
  Util::LogExit scopedLog("GRACEFUL");
  MqttWrapper libmMosquitto;

  YAML::Node config = YAML::LoadFile("config.yaml");
  dumpYaml(config);

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
