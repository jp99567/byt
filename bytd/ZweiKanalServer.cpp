#include "ZweiKanalServer.h"

#include "Log.h"

ZweiKanalServer::ZweiKanalServer(boost::asio::io_service &io_service)
    :io_service(io_service)
    ,acceptor(io_service, tcp::endpoint(tcp::v4(), 1981))
{
}

ZweiKanalServer::~ZweiKanalServer()
{
    LOG_INFO("destruct ~ZweiKanalServer");
}

void ZweiKanalServer::listen()
{
    LOG_INFO("listen");

    ioThread = std::thread([this]{
	  start_accept();
	  io_service.run();
	  LOG_INFO("io thread finished");
  });
}

void ZweiKanalServer::interrupt()
{
    LOG_INFO("accept interrupt requested by server");
    {
        std::unique_lock<std::mutex> lk(lock);
        interruptReq = true;
        cvaccept.notify_all();
    }
}

void ZweiKanalServer::interruptChildren()
{
    LOG_INFO("accept interrupt children not implemented");
}

void ZweiKanalServer::close()
{
    LOG_INFO("close server");
    io_service.stop();
    ioThread.join();
}

boost::shared_ptr<apache::thrift::transport::TTransport> ZweiKanalServer::acceptImpl()
{
    std::unique_lock<std::mutex> lk(lock);
    cvaccept.wait(lk, [this]{return !conns.empty() || interruptReq});
    if(!interruptReq)
    {
        return boost::shared_ptr<TTransport>(nullptr);
    }
    else
    {
        while(!conns.empty()) conns.pop();
        interruptReq = false;
        using apache::thrift::transport::TTransportException;
        TTransportException e(TTransportException::TTransportExceptionType::INTERRUPTED);
        throw e;
    }
}

void ZweiKanalServer::start_accept()
{
    auto new_connection = tcp_connection::create(io_service, *this);
    acceptor.async_accept(new_connection->socket(), [this,new_connection](const boost::system::error_code& error){
        handle_accept(new_connection, error);
    });
}

void ZweiKanalServer::handle_accept(tcp_connection::sPtr new_connection, const boost::system::error_code &error)
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

void ZweiKanalServer::addVerified(tcp_connection::sPtr connection)
{
    std::unique_lock<std::mutex> guard(lock);
    conns.emplace(connection);
    cvaccept.notify_one();
}
