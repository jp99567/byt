#pragma once
#include <linux/can.h>
#include <mutex>
#include <functional>

class CanBus
{
    int socket = -1;
    can_frame frame_wr;
    bool pending = false;
    std::mutex lock;

public:
    CanBus();
    ~CanBus();
    bool send(const can_frame& frame);
    void setOnRecv(std::function<void(const can_frame& frame)> cbk);
    void setWrReadyCbk(std::function<void()> cbk);
private:
    bool read(can_frame& frame);
    bool write(const can_frame& frame);
    std::function<void(const can_frame& frame)> onRecv;
    std::function<void()> wrReady;
};

