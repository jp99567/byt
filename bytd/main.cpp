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
#include <string>
#include <condition_variable>
#include <boost/make_shared.hpp>
#include <boost/asio.hpp>
#include <thrift/transport/TServerTransport.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <spdlog/spdlog.h>
#include "gen-req/BytRequest.h"
#include "lbidich/packet.h"
#include "lbidich/iconnection.h"


std::shared_ptr<spdlog::logger> logger() {
	static auto _ = spdlog::stdout_color_mt("console");
	return _;
}

#define LOG_INFO(...) logger()->info(__VA_ARGS__)

using boost::asio::ip::tcp;
using apache::thrift::transport::TTransport;

class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
public:
  using sPtr = std::shared_ptr<tcp_connection>;

  static sPtr create(boost::asio::io_service& io_service)
  {
    return sPtr(new tcp_connection(io_service));
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start()
  {
	  do_read();
/*
    boost::asio::async_write(socket_, boost::asio::buffer(std::string("ABC")),
    		[this](const boost::system::error_code& error,
      size_t bytes_transferred){
    	handle_write(error, bytes_transferred);
    });*/
  }

  ~tcp_connection()
    {
  	  LOG_INFO("destruct tcp:connection");
    }

  void do_read()
  {
	  auto self = shared_from_this();
	  socket_.async_read_some(boost::asio::buffer(bufIn),
	  	  [this,self](const boost::system::error_code& error, size_t bytes_transferred){
		  	  if(!error){
		  		  LOG_INFO("read: {}", bytes_transferred);
	  			  auto ptr = std::cbegin(bufIn);
	  			  auto end = std::cbegin(bufIn) + bytes_transferred;
	  			  do{
	  				   ptr = packetIn.load(ptr, end-ptr);
	  				   if(ptr){
	  					   auto id = (lbidich::ChannelId)packetIn.getHeader().chId;
	  					   onNewPacket(id, packetIn.getMsg());
	  				   }
	  				   if(ptr == end)
	  					   break;
	  			  }
	  			  while(ptr);

	  			  do_read();
	  		  }
	  		  else{
	  			  LOG_INFO("read error {}:{}:{}", error.category().name(),
	  					  error.message(),
	  					  error.value());
	  		  }
	  	  });
  }

  void onNewPacket(lbidich::ChannelId ch, lbidich::DataBuf buf)
  {
	  switch(ch)
	  {
	  	  case lbidich::ChannelId::auth:
	  		  LOG_INFO("auth:{}", buf.size());
	  		  break;
	  	  default:
	  		  LOG_INFO("channel unknown {}", int(ch));
	  		  break;
	  }
  }

private:
  tcp_connection(boost::asio::io_service& io_service)
    : socket_(io_service)
  {
  }

  void handle_write(const boost::system::error_code& error,
      size_t bytes_transferred)
  {

  }

  tcp::socket socket_;
  lbidich::PacketIn packetIn;
  std::array<uint8_t, 64> bufIn;
};

class ZweiKanalServer : public apache::thrift::transport::TServerTransport
{
public:

	ZweiKanalServer(boost::asio::io_service& io_service)
	:io_service(io_service)
    ,acceptor(io_service, tcp::endpoint(tcp::v4(), 1981))
	{
	}

	~ZweiKanalServer()
	{
		LOG_INFO("destruct ~ZweiKanalServer");
	}

  /**
   * Starts the server transport listening for new connections. Prior to this
   * call most transports will not return anything when accept is called.
   *
   * @throws TTransportException if we were unable to listen
   */
  void listen() override
  {
	  LOG_INFO("listen");

	  ioThread = std::thread([this]{
		  start_accept();
		  io_service.run();
		  LOG_INFO("io thread finished");
	  });
  }

  /**
   * For "smart" TServerTransport implementations that work in a multi
   * threaded context this can be used to break out of an accept() call.
   * It is expected that the transport will throw a TTransportException
   * with the INTERRUPTED error code.
   *
   * This will not make an attempt to interrupt any TTransport children.
   */
  void interrupt() override
  {
	  LOG_INFO("accept interrupt requested by server");
	  {
		  std::unique_lock<std::mutex> lk(lock);
		  acceptState = AcceptState::end;
		  cvaccept.notify_all();
	  }
  }

  /**
   * This will interrupt the children created by the server transport.
   * allowing them to break out of any blocking data reception call.
   * It is expected that the children will throw a TTransportException
   * with the INTERRUPTED error code.
   */
  void interruptChildren() override
  {
	  LOG_INFO("accept interrupt children not implemented");
  }

  /**
   * Closes this transport such that future calls to accept will do nothing.
   */
  void close() override
  {
	  LOG_INFO("close server");
	  io_service.stop();
	  ioThread.join();
  }

protected:
  boost::shared_ptr<TTransport> acceptImpl() override
  {
	  std::unique_lock<std::mutex> lk(lock);
	  cvaccept.wait(lk, [this]{return acceptState != AcceptState::wait;});
	  if(acceptState == AcceptState::new_con)
	  {
		  return boost::shared_ptr<TTransport>(nullptr);
	  }
	  else
	  {
		  using apache::thrift::transport::TTransportException;
		  TTransportException e(TTransportException::TTransportExceptionType::INTERRUPTED);
		  throw e;

	  }
  }

private:

  void start_accept()
  {
	  auto new_connection = tcp_connection::create(io_service);
	  acceptor.async_accept(new_connection->socket(), [this,new_connection](const boost::system::error_code& error){
		   	handle_accept(new_connection, error);
	   });
  }

  void handle_accept(tcp_connection::sPtr new_connection,
      const boost::system::error_code& error)
  {
    if (!error){
      LOG_INFO("new connection");
      new_connection->start();
    }
    else{
    	LOG_INFO("accept error {}:{}", error.category().name(), error.value());
    }

    start_accept();
  }

  enum class AcceptState { wait, new_con, end } acceptState = AcceptState::wait;
  std::thread ioThread;
  boost::asio::io_service& io_service;
  tcp::acceptor acceptor;
  std::mutex lock;
  std::condition_variable cvaccept;
};

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
