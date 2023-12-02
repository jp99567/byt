#pragma once

#include <thread>
#include "SimpleSensor.h"
#include "rekuperacia_fw/reku_enum.h"

namespace reku { struct RekuTx; }

struct pollfd;

class Reku
{
public:
    explicit Reku(IMqttPublisherSPtr mqtt, const char* ttydev = "/dev/ttyUSB0");
    ~Reku();
    float FlowPercent = 30;
    float ByPassTemp = 25;
    bool bypass = false;
    struct PVs
    {
        std::array<float, 2> rpm;
        std::array<float, 2> temp;
        bool bypass;
    } pvs;
    const PVs getPV() const {return pvs;}
private:
    bool readFrame(reku::RekuTx& recvFrame, struct pollfd& pfd);
    int fd=-1;
    std::thread thread;
    bool commOk = false;
    unsigned timeout_cnt = 0;
    std::array<SimpleSensor, reku::_LAST> temperature;
    std::array<SimpleSensor, reku::_LAST> rpm;
};
