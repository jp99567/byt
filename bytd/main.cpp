
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
#include "BytRequest.h"
#include "ZweiKanalServer.h"
#include "exporter.h"
#include "therm.h"

class SimpleGpio
{
public:
	SimpleGpio(std::string name)
	:dir(std::string("/run /gpio/") + name + '/')
	{
		readdirection();
		readvalue();
	}

	void set(bool hi)
	{
		if(isOutout()){
			writef("value", hi ? "1" : "0");
		}
		else{
			writef("direction", hi ? "high" : "low");
			direction = true;
		}
		val = hi;
	}

	void setInput()
	{
		writef("direction", "in");
		direction = false;
	}

	bool operator()()
	{
		if(isOutout())
		{
			return val;
		}

		return readvalue();
	}

private:
	std::string readf(std::string fname)
	{
		std::string v;
		std::ifstream f(dir+fname);
		f >> v;
		return v;
	}

	void writef(std::string fname, std::string v)
	{
		std::ofstream f(dir+fname);
		f << v;
	}

	bool readdirection()
	{
		auto d = readf("direction");
		if( d == "in" )
		{
			direction = false;
		}
		else if( d == "out" )
		{
			direction = true;
		}
		else{
			throw std::runtime_error("gpio invalid direction");
		}

		return direction;
	}

	bool readvalue()
	{
		auto v = readf("value");
		if( v == "0" )
		{
			val = false;
		}
		else if( v == "1" )
		{
			val = true;
		}
		else{
			throw std::runtime_error("gpio invalid value");
		}

		return val;
	}

	bool isOutout() const { return direction; }

	const std::string dir;
	bool direction = false; // false IN, true OUT
	bool val = false;
};

class Facade : public ICore
{
public:
	explicit Facade()
	:ring("ring"){}

	bool sw(int id, bool on) final
	{
		ring.set(id==1);
		++stat;
		return true;
	}

	bool cmd(int id) final
	{
		return true;
	}

	unsigned status()  final
	{
		ring.set(false);
		return stat;
	}

private:
	unsigned stat = 0;
	SimpleGpio ring;
};

// alza.cz: Vazeny zakaznik,
//objednavku 178330005 sme pre Vas pripravili na centrale Alza.sk.
//Do Payboxu zadajte cislo objednavky a PIN 3992.

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
		meranie = std::make_shared<MeranieTeploty>();

		if( ! meranie->init(exporter) )
		{
			throw std::runtime_error("meranie teploty init failed");
		}
    }

	void run()
	{
		bool running = true;
		std::mutex cvlock;
		std::condition_variable cv_running;

		std::thread meastempthread([this,&running,&cv_running,&cvlock]{
			do{
				meranie->meas();
				std::unique_lock<std::mutex> ulck(cvlock);
				cv_running.wait_for(ulck, std::chrono::seconds(10), [running]{
					return !running;
				});
			}
			while(running);
		});

		server.serve();

		{
			std::unique_lock<std::mutex> guard(cvlock);
			running = true;
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
      LogERR("crash {}", e.what());
      return 1;
  }
  catch (...)
  {
       LogERR("crash unknown");
       return 1;
  }

  return 0;
}
