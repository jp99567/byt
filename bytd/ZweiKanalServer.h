#pragma once


#include <thrift/transport/TServerTransport.h>
#include <boost/asio.hpp>
#include <condition_variable>
#include <thread>
#include <queue>
#include <set>
#include "IconnServer.h"
#include "TcpConnection.h"

using boost::asio::ip::tcp;
using apache::thrift::transport::TTransport;

class ZweiKanalServer : public apache::thrift::transport::TServerTransport, public IConnServer
{
public:

    ZweiKanalServer(boost::asio::io_service& io_service);
    static boost::asio::ip::tcp::acceptor createAcceptor(boost::asio::io_service &io_service);
    ~ZweiKanalServer();

  /**
   * Starts the server transport listening for new connections. Prior to this
   * call most transports will not return anything when accept is called.
   *
   * @throws TTransportException if we were unable to listen
   */
  void listen() override;

  /**
   * For "smart" TServerTransport implementations that work in a multi
   * threaded context this can be used to break out of an accept() call.
   * It is expected that the transport will throw a TTransportException
   * with the INTERRUPTED error code.
   *
   * This will not make an attempt to interrupt any TTransport children.
   */
  void interrupt() override;

  /**
   * This will interrupt the children created by the server transport.
   * allowing them to break out of any blocking data reception call.
   * It is expected that the children will throw a TTransportException
   * with the INTERRUPTED error code.
   */
  void interruptChildren() override;

  /**
   * Closes this transport such that future calls to accept will do nothing.
   */
  void close() override;

protected:
  std::shared_ptr<TTransport> acceptImpl() override;

private:

  void start_accept();

  void handle_accept(tcp_connection::sPtr new_connection,
      const boost::system::error_code& error);

  void addVerified(tcp_connection::sPtr connection) override;
  void rmFrom(tcp_connection::sPtr connection) override;

  bool interruptReq = false;
  std::thread ioThread;
  boost::asio::io_service& io_service;
  tcp::acceptor acceptor;
  std::mutex lock;
  std::condition_variable cvaccept;
  std::queue<tcp_connection::sPtr> conns;
  std::set<std::weak_ptr<tcp_connection>, std::owner_less<std::weak_ptr<tcp_connection>>> activeConns;
};
