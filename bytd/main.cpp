
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

public:
	AppContainer()
	:signals(io_service, SIGINT, SIGTERM)
    {
		signals.async_wait([this](auto error, auto signum){
			LogINFO("signal {}",signum);
            io_service.stop();
		});
		exporter = std::make_shared<ow::Exporter>();
		auto pru = std::make_shared<Pru>();
		meranie = std::make_shared<MeranieTeploty>(pru, exporter);
		openTherm = std::make_shared<OpenTherm>(pru);
    }

	void run()
	{
		bool running = true;
		std::mutex cvlock;
		std::condition_variable cv_running;

		std::thread meastempthread([this,&running,&cv_running,&cvlock]{
			thread_util::set_thread_name("bytd-meranie");

			Elektromer em;
			do{
				meranie->meas();
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

			}
			while(running);
		});

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

int main()
{
  Util::LogExit scopedLog("GRACEFUL");

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
