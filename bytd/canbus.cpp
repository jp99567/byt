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

void CanBus::read(can_frame &frame) const
{
    if(socket == -1)
        return;
    auto len = ::read(socket, &frame, sizeof(struct can_frame));
    if( len < (int)sizeof(struct can_frame)) {
        LogSYSDIE("CAN read");
    }
}

void CanBus::write(const can_frame &frame) const
{
    if(socket == -1)
        return;
    auto len = ::write(socket, &frame, sizeof(struct can_frame));
    if( len < (int)sizeof(struct can_frame)) {
        LogSYSDIE("CAN write");
    }
}
