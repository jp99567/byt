#include "TcpConnection.h"
#include "Log.h"
#include "IconnServer.h"
#include "lbidich/channel.h"
#include "lbidich/packet.h"
#include <boost/exception/all.hpp>

void tcp_connection::start()
{
    do_read();
}

tcp_connection::~tcp_connection()
{
    LogINFO("destruct tcp:connection {}", connectionInfo());
}

void tcp_connection::do_read()
{
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(bufIn),
                            [this,self](const boost::system::error_code& error, size_t bytes_transferred){
        if(!error){
            LogINFO("read: {}", bytes_transferred);
            auto ptr = std::cbegin(bufIn);
            auto end = std::cbegin(bufIn) + bytes_transferred;

            do{
                ptr = packetIn.load(ptr, end-ptr);
                if(ptr){
                    auto id = (lbidich::ChannelId)packetIn.getHeader().chId;
                    if(!onNewPacket(id, packetIn.getMsg())){
                        onClose();
                        connServ.rmFrom(shared_from_this());
                    	return;
                    }
                }
                if(ptr == end)
                    break;
            }
            while(ptr);

            do_read();
        }
        else{
            LogINFO("read error {}:{}:{}", error.category().name(),
                     error.message(),
                     error.value());
            onClose();
            connServ.rmFrom(shared_from_this());
        }
    });
}

bool tcp_connection::onNewPacket(lbidich::ChannelId ch, lbidich::DataBuf buf)
{
    switch(ch)
    {
    case lbidich::ChannelId::auth:
    {
        std::string authMsg(std::begin(buf), std::end(buf)-1);
        LogINFO("auth:{} {}", buf.size(), authMsg);
        if(authMsg == "Na Straz!")
        {
            connServ.addVerified(shared_from_this());
        }
        else
        {
        	LogERR("auth failed {}", connectionInfo());
        	return false;
        }
    }
        break;
    case lbidich::ChannelId::down:
    case lbidich::ChannelId::up:
        return lbidich::IoBase::onNewPacket(ch, std::move(buf));
    default:
        LogERR("channel unknown {}", int(ch));
        break;
    }
    return true;
}

std::string tcp_connection::connectionInfo() const
{
	  try
	  {
		  if(socket_.is_open())
		  	return socket_.remote_endpoint().address().to_string();
	  }
	  catch (boost::exception &e)
	  {
	    LogERR("boost ex:{}", boost::diagnostic_information(e));
	  }

    return "invalid";
}


std::shared_ptr<apache::thrift::transport::TTransport> tcp_connection::getClientChannel()
{
    auto channel = std::make_unique<lbidich::Channel>(lbidich::ChannelId::down, shared_from_this());
    return std::shared_ptr<apache::thrift::transport::TTransport>(
               new BytTransport(std::move(channel)));
}

bool tcp_connection::put(lbidich::ChannelId chId, const uint8_t* msg, unsigned int len)
{
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(lbidich::packMsg((uint8_t)chId, msg, len)),
                [this,self](const boost::system::error_code& error, size_t bytes_transferred)
    {
        if(!error){
            LogINFO("write: {}", bytes_transferred);
        }
        else{
            LogERR("write error {}:{}:{}", error.category().name(),
                     error.message(),
                     error.value());
        }
    });
    return true;
}
