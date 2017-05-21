//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <boost/make_shared.hpp>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include "Log.h"
#include "ZweiKanalServer.h"
#include "gen-req/BytRequest.h"

using boost::asio::ip::tcp;
using apache::thrift::transport::TTransport;

class BytResponserFactory : public doma::BytRequestIfFactory
{
public:
	doma::BytRequestIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
	{
		  return new doma::BytRequestNull;
	}

	void releaseHandler(doma::BytRequestIf* p) override
	{
		  delete p;
	}
};

class AppContainer
{
	boost::asio::io_service io_service;
	boost::asio::signal_set signals;
	boost::shared_ptr<ZweiKanalServer> serverTransport;
	apache::thrift::server::TThreadedServer server;
public:
	AppContainer()
	:signals(io_service, SIGINT, SIGTERM)
	,serverTransport(boost::make_shared<ZweiKanalServer>(io_service))
	,server(boost::make_shared<doma::BytRequestProcessorFactory>(boost::make_shared<BytResponserFactory>()),
		    serverTransport,
		    boost::make_shared<apache::thrift::transport::TTransportFactory>(),
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

private:
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
