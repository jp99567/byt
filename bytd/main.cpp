
#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>

#include "Log.h"
#include "BytRequest.h"
#include "ZweiKanalServer.h"

class AppContainer
{
	boost::asio::io_service io_service;
	boost::asio::signal_set signals;
	apache::thrift::server::TThreadedServer server;

public:
	AppContainer()
	:signals(io_service, SIGINT, SIGTERM)
	,server(std::make_shared<doma::BytRequestProcessorFactory>(std::make_shared<BytRequestFactory>()),
			std::make_shared<ZweiKanalServer>(io_service),
		    std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
		    std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>())
    {
		signals.async_wait([this](auto error, auto signum){
			LogINFO("signal {}",signum);
			server.stop();
		});
    }

	void run()
	{
		server.serve();
	}
};

int main()
{
  try
  {
      AppContainer().run();
      LogINFO("GRACEFULLY");
  }
  catch (std::exception& e)
  {
      LogERR("crash {}", e.what());
      return 1;
  }

  return 0;
}
