#include "TcpConnection.h"
#include "Log.h"
#include "IconnServer.h"
#include "lbidich/channel.h"

void tcp_connection::start()
{
    do_read();
    /*
    boost::asio::async_write(socket_, boost::asio::buffer(std::string("ABC")),
                [this](const boost::system::error_code& error,
      size_t bytes_transferred){
        handle_write(error, bytes_transferred);
    });*/
}

tcp_connection::~tcp_connection()
{
    LOG_INFO("destruct tcp:connection {}", connectionInfo());
}

void tcp_connection::do_read()
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
                    if(!onNewPacket(id, packetIn.getMsg()))
                    	return;
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

bool tcp_connection::onNewPacket(lbidich::ChannelId ch, lbidich::DataBuf buf)
{
    switch(ch)
    {
    case lbidich::ChannelId::auth:
    {
        std::string authMsg(std::begin(buf), std::end(buf)-1);
        LOG_INFO("auth:{} {}", buf.size(), authMsg);
        if(authMsg == "Na Straz!")
        {
            connServ.addVerified(shared_from_this());
        }
        else
        {
        	LOG_ERR("auth failed {}", connectionInfo());
        	return false;
        }
    }
        break;
    case lbidich::ChannelId::down:
    case lbidich::ChannelId::up:
        return lbidich::IoBase::onNewPacket(ch, std::move(buf));
    default:
        LOG_ERR("channel unknown {}", int(ch));
        break;
    }
    return true;
}

std::string tcp_connection::connectionInfo() const
{
	return "TODO";
}


boost::shared_ptr<apache::thrift::transport::TTransport> tcp_connection::getClientChannel()
{
    auto channel = std::make_unique<lbidich::Channel>(lbidich::ChannelId::down, shared_from_this());
    return boost::shared_ptr<apache::thrift::transport::TTransport>(
               new BytTransport(std::move(channel)));
}

