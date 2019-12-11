
#include <systemd/sd-daemon.h>
#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "Log.h"
#include "thread_util.h"
#include "BytRequest.h"
#include "ZweiKanalServer.h"
#include "exporter.h"
#include "therm.h"
#include "Elektromer.h"
#include "Pru.h"

class Facade : public ICore
{
public:
	explicit Facade()
	{}

	bool sw(int id, bool on) final
	{
		++stat;
		return true;
	}

	bool cmd(int id) final
	{
		return true;
	}

	unsigned status()  final
	{
		return stat;
	}

private:
	unsigned stat = 0;
};

class AppContainer
{
	boost::asio::io_service io_service;
	boost::asio::signal_set signals;
	apache::thrift::server::TThreadedServer server;
	ow::ExporterSptr exporter;
	std::shared_ptr<MeranieTeploty> meranie;

public:
	AppContainer()
	:signals(io_service, SIGINT, SIGTERM)
	,server(std::make_shared<doma::BytRequestProcessorFactory>(std::make_shared<BytRequestFactory>(std::make_shared<Facade>())),
			std::make_shared<ZweiKanalServer>(io_service),
		    std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
		    std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>())
    {
		signals.async_wait([this](auto error, auto signum){
			LogINFO("signal {}",signum);
			server.stop();
		});
		exporter = std::make_shared<ow::Exporter>();
		auto pru = std::make_shared<Pru>();
		meranie = std::make_shared<MeranieTeploty>(pru, exporter);
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
			}
			while(running);
		});

		sd_notify(0, "READY=1");

		server.serve();
		LogINFO("thrift server exited");

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
