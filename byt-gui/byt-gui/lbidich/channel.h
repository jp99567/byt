#pragma once

#include "iconnection.h"

namespace lbidich {

class Channel : public IConnection
{
public:
    Channel(ChannelId chId, IIo& io)
        :chId(chId)
        ,io(io){}

    bool isOpen() const override
    {
        return opened;
    }

    virtual bool send(const uint8_t* data, unsigned size)
    {
        return io.put(chId, data, size);
    }

    int recv(uint8_t* data, unsigned size) override
    {
        std::unique_lock<std::mutex> guard(lck);
        cv.wait(guard, [this]{return !msgs.empty() || !opened;});
        if(!opened)
            return false;

        auto req = size;
        while(req)
        {
            auto& buf = msgs.front();
            if(req < buf.size())
            {
                std::copy_n(std::cbegin(buf), size, data);
                buf.erase(std::cbegin(buf), std::cbegin(buf)+size);
                return size;
            }
            else
            {
                std::copy(std::cbegin(buf), std::cend(buf), data);
                data += buf.size();
                req -= buf.size();
                msgs.pop();
            }

            if(msgs.empty())
            {
                return size-req;
            }
        }
        return size;
    }

    void onNewMsg(lbidich::DataBuf msg)
    {
        std::unique_lock<std::mutex> guard(lck);
        opened = msg.size() > 0;
        msgs.emplace(std::move(msg));
        cv.notify_one();
    }

    bool open() override
    {
        return opened;
    }

    bool close() override
    {
        return false;
    }

private:
    const ChannelId chId;
    std::mutex lck;
    std::condition_variable cv;
    std::queue<lbidich::DataBuf> msgs;
    bool opened = true;
    IIo& io;
};
}
