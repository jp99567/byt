#pragma once

#include "SimpleSensor.h"
#include "rekuperacia_fw/reku_enum.h"
#include <thread>

namespace reku {
struct RekuTx;
struct RekuRx;
}

struct pollfd;

class Reku {
public:
    explicit Reku(IMqttPublisherSPtr mqtt, const char* ttydev = "/dev/ttyUSB0");
    ~Reku();
    float FlowPercent = 30;
    float FlowExhaustPercent = std::numeric_limits<float>::quiet_NaN();

private:
    bool bypass = false;
    void initFd(const char* ttydev = "/dev/ttyUSB0");
    bool readFrame(reku::RekuTx& recvFrame, struct pollfd& pfd);
    void onNewData(const reku::RekuTx& recvFrame);
    bool write(const reku::RekuRx& frame);
    void onFailure();
    int fd = -1;
    std::thread thread;
    bool commOk = false;
    unsigned timeout_cnt = 0;
    std::array<SimpleSensor, reku::_LAST> temperature;
    std::array<SimpleSensor, reku::_LAST> rpm;
};
