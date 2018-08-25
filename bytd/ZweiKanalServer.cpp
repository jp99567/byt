#include "ZweiKanalServer.h"
#include "lbidich/byttransport.h"

#include "Log.h"

ZweiKanalServer::ZweiKanalServer(boost::asio::io_service &io_service)
    :io_service(io_service)
    ,acceptor(io_service, tcp::endpoint(tcp::v4(), 1981))
{
}

ZweiKanalServer::~ZweiKanalServer()
{
    LogINFO("destruct ~ZweiKanalServer");
}

void ZweiKanalServer::listen()
{
    LogINFO("listen");

    ioThread = std::thread([this]{
	  start_accept();
	  io_service.run();
	  LogINFO("io thread finished");
  });
}

void ZweiKanalServer::interrupt()
{
    LogINFO("accept interrupt requested by server");
    {
        std::unique_lock<std::mutex> lk(lock);
        interruptReq = true;
        cvaccept.notify_all();
    }
}

void ZweiKanalServer::interruptChildren()
{
    LogINFO("accept interrupt children");
    lock.lock();
    auto tmpCopy = std::move(activeConns);
    lock.unlock();
    
    for(auto p : tmpCopy)
    {
        auto sp = p.lock();
        if(sp)
            sp->onClose();
    }
}

void ZweiKanalServer::close()
{
    LogINFO("close server");
    io_service.stop();
    ioThread.join();
}

std::shared_ptr<apache::thrift::transport::TTransport> ZweiKanalServer::acceptImpl()
{
    std::unique_lock<std::mutex> lk(lock);
    cvaccept.wait(lk, [this]{return !conns.empty() || interruptReq;});
    if(!interruptReq)
    {
    	if(!conns.empty()){
    		auto tcp = conns.front();
    		conns.pop();
                activeConns.emplace(tcp);
    		return tcp->getClientChannel();
    	}
        return std::shared_ptr<TTransport>(nullptr);
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
        LogINFO("new connection");
        new_connection->start();
    }
    else{
        LogINFO("accept error {}:{}", error.category().name(), error.value());
    }

    start_accept();
}

void ZweiKanalServer::addVerified(tcp_connection::sPtr connection)
{
    std::unique_lock<std::mutex> guard(lock);
    conns.emplace(connection);
    cvaccept.notify_one();
}

void ZweiKanalServer::rmFrom(tcp_connection::sPtr connection)
{
    std::unique_lock<std::mutex> guard(lock);
    std::weak_ptr<tcp_connection> wp = connection;
    
    auto it = activeConns.find(wp);
    
    if( it != std::end(activeConns) )
        activeConns.erase(it);
}
