#pragma once

#include "iconnection.h"
#include <memory>

namespace lbidich {

class Channel : public IConnection
{
public:
    Channel(ChannelId chId, std::shared_ptr<IIo> io)
        :chId(chId)
        ,io(io)
    {
        io->setOnNewMsgCbk(chId, [this](DataBuf msg){
            onNewMsg(std::move(msg));
        });
    }

    ~Channel()
    {
        io->setOnNewMsgCbk(chId,[](DataBuf){});
        onNewMsg({});
    }

    bool isOpen() const override
    {
        return opened;
    }

    virtual bool send(const uint8_t* data, unsigned size)
    {
        opened = io->put(chId, data, size);
        return opened;
    }

    int recv(uint8_t* data, unsigned size) override
    {
        std::unique_lock<std::mutex> guard(lck);
        cv.wait(guard, [this]{return !msgs.empty() || !opened;});
        if(!opened)
            return -1;

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

    void onNewMsg(DataBuf msg)
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
    std::queue<DataBuf> msgs;
    bool opened = true;
    std::shared_ptr<IIo> io;
};
}
