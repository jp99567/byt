#include "canbus.h"

#include "Log.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <boost/asio/buffer.hpp>


CanBus::CanBus(boost::asio::io_service& io_context)
    :canStream(io_context)
    ,onRecv([](const can_frame& msg){LogERR("can recv:{:X} not connected callback", msg.can_id);})
    ,wrReady([]{LogERR("can write ready - not connected callback");})
{
    struct sockaddr_can addr;
    struct ifreq ifr;

    auto socket = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(socket < 0) {
        LogSYSDIE("CAN Socket");
    }

    strcpy(ifr.ifr_name, "can1" );
    auto rv = ::ioctl(socket, SIOCGIFINDEX, &ifr);
    if( rv == -1){
        LogSYSDIE("CAN Socket ioctl");
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    rv = ::bind(socket, (struct sockaddr *)&addr, sizeof(addr));
    if( rv == -1){
        LogSYSDIE("CAN Socket ioctl");
    }

    canStream.assign(socket);
    canStream.non_blocking(true);
    read();
}

CanBus::~CanBus()
{
}

void CanBus::setOnRecv(std::function<void (const can_frame &)> cbk)
{
    std::lock_guard<std::mutex> ulck(lock);
    onRecv = cbk;
}

void CanBus::setWrReadyCbk(std::function<void ()> cbk)
{
    std::lock_guard<std::mutex> ulck(lock);
    wrReady = cbk;
}

bool operator==(const can_frame &a, const can_frame &b)
{
    if(a.can_id == b.can_id && a.can_dlc == b.can_dlc){
        if(0 == ::memcmp(a.data, b.data, a.can_dlc))
                return true;
    }
    return false;
}

void CanBus::read()
{
    canStream.async_read_some(boost::asio::buffer(&frame_rd, sizeof(frame_rd)), [this](const boost::system::error_code& ec, std::size_t bytes_transferred){
        if(ec || bytes_transferred != sizeof(frame_rd)){
            LogERR("can read ({}){} size:{}/{}", ec.value(), ec.message(), bytes_transferred, sizeof(frame_rd));
        }
        else{
            bool isWrReady = false;
            {
                std::lock_guard<std::mutex> ulck(lock);
                if(pending){
                    if(frame_rd == frame_wr){
                        pending = false;
                        isWrReady = true;
                    }
                }
            }
            if(isWrReady){
                LogDBG("can write read back ok");
                wrReady();
            }
            else{
                LogDBG("can async read ok");
                onRecv(frame_rd);
            }
            read();
        }
    });
}

bool CanBus::send(const can_frame &frame)
{
    std::lock_guard<std::mutex> ulck(lock);
    if( not pending){
        pending = true;
        frame_wr = frame;
        canStream.async_write_some(boost::asio::buffer(&frame_wr, sizeof(frame_wr)), [this](const boost::system::error_code& ec, std::size_t bytes_transferred){
            if(ec || bytes_transferred != sizeof(frame_rd)){
                LogERR("can write ({}){}", ec.value(), ec.message());
            }
            else{
                LogDBG("can async write handler ok");
            }
        });
        return true;
    }
    return false;
}

int testapp()
{
    boost::asio::io_service io_context;
    CanBus bus(io_context);
    bus.setOnRecv([](const can_frame& frame){
        LogINFO("recv can frame {:X} {}", frame.can_id, frame.data);
    });
    return 0;
}

#ifdef TESTAPP
int main()
{
    return testapp();
}
#endif
