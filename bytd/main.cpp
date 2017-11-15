
#include <iostream>
#include <boost/make_shared.hpp>
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
	,server(boost::make_shared<doma::BytRequestProcessorFactory>(boost::make_shared<BytRequestFactory>()),
			boost::make_shared<ZweiKanalServer>(io_service),
		    boost::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
		    boost::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>())
    {
		signals.async_wait([this](auto error, auto signum){
			LOG_INFO("signal {}",signum);
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
  }
  catch (std::exception& e)
  {
      std::cerr << e.what() << std::endl;
  }

  return 0;
}
