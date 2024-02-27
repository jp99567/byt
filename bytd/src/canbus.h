#pragma once
#include <boost/asio/io_service.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <functional>
#include <linux/can.h>
#include <mutex>

class CanBus {
    boost::asio::posix::stream_descriptor canStream;
    can_frame frame_wr;
    can_frame frame_rd;
    bool pending = false;
    std::mutex lock;

public:
    explicit CanBus(boost::asio::io_service& io_context);
    ~CanBus();
    bool send(const can_frame& frame);
    void setOnRecv(std::function<void(const can_frame& frame)> cbk);
    void setWrReadyCbk(std::function<void()> cbk);

private:
    void read();
    std::function<void(const can_frame& frame)> onRecv;
    std::function<void()> wrReady;
};
