#pragma once

#include <memory>
#include <boost/asio.hpp>
#include "lbidich/packet.h"
#include "lbidich/iconnection.h"

using boost::asio::ip::tcp;

class IConnServer;

class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
public:
  using sPtr = std::shared_ptr<tcp_connection>;

  static sPtr create(boost::asio::io_service& io_service, IConnServer& connServ)
  {
    return sPtr(new tcp_connection(io_service, connServ));
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start();

  ~tcp_connection();

private:
  tcp_connection(boost::asio::io_service& io_service,  IConnServer& connServ)
    :socket_(io_service)
    ,connServ(connServ)
  {
  }

  void handle_write(const boost::system::error_code& error,
      size_t bytes_transferred)
  {
  }

  void do_read();
  bool onNewPacket(lbidich::ChannelId ch, lbidich::DataBuf buf);
  std::string connectionInfo() const;

  tcp::socket socket_;
  lbidich::PacketIn packetIn;
  std::array<uint8_t, 64> bufIn;
  IConnServer& connServ;
};
