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

CanBus::CanBus()
    :onRecv([](const can_frame&){})
    ,wrReady([]{})
{
    struct sockaddr_can addr;
    struct ifreq ifr;

    socket = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(socket < 0) {
        LogSYSDIE("CAN Socket");
    }

    strcpy(ifr.ifr_name, "vcan0" );
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
}

CanBus::~CanBus()
{
    auto rv = ::close(socket);
    if( rv == -1){
        LogSYSDIE("CAN Socket close");
    }
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

bool CanBus::read(can_frame &frame)
{
    if(socket == -1)
        return false;
    auto len = ::read(socket, &frame, sizeof(struct can_frame));
    if( len < (int)sizeof(struct can_frame)) {
        LogSYSDIE("CAN read");
    }
    bool isWrReady = false;
    {
        std::lock_guard<std::mutex> ulck(lock);
        if(pending){
            if(frame == frame_wr){
                pending = false;
                isWrReady = true;
            }
        }
    }
    if(isWrReady){
        wrReady();
        return false;
    }
    onRecv(frame);
    return true;
}

bool CanBus::write(const can_frame &frame)
{
    if(socket == -1)
        return false;
    std::lock_guard<std::mutex> ulck(lock);
    if( not pending){
        pending = true;
        frame_wr = frame;
        auto len = ::write(socket, &frame_wr, sizeof(struct can_frame));
        if( len < (int)sizeof(struct can_frame)) {
            LogSYSDIE("CAN write");
        }
        return true;
    }
    return false;
}
