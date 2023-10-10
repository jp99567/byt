#pragma once

#include <thread>

class Reku
{
public:
    explicit Reku(const char* ttydev = "/dev/ttyUSB0");
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
    int fd=-1;
    std::thread thread;
    bool commOk = true;
};
