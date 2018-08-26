#pragma once

#include "iconnection.h"
#include <memory>
#include  <condition_variable>
#include <queue>

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

    bool send(const uint8_t* data, unsigned size) override
    {
        if(opened){
            opened = io->put(chId, data, size);
            if(!opened)
                closing = true;
        }
        return opened;
    }

    int recv(uint8_t* data, unsigned size) override
    {
        std::unique_lock<std::mutex> guard(lck);
        cv.wait(guard, [this]{return !msgs.empty() || !opened;});
        if(!opened)
        {
            if(closing)
            {
                closing = false;
                return 0;
            }
            return -1;
        }

        auto req = size;
        while(req)
        {
            auto& buf = msgs.front();
            
            if(buf.size() == 0)
            {
                opened = false;
                closing = size > req;
                do{
                    msgs.pop();
                }
                while(!msgs.empty());

                return size-req;
            }
            
            if(req < buf.size())
            {
                std::copy_n(std::begin(buf), size, data);
                buf.erase(std::begin(buf), std::begin(buf)+size);
                return size;
            }
            else
            {
                std::copy(std::begin(buf), std::end(buf), data);
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
        msgs.emplace(std::move(msg));
        cv.notify_one();
    }

    bool open() override
    {
        return opened;
    }

    bool close() override
    {
        std::unique_lock<std::mutex> guard(lck);
        opened = false;;
        cv.notify_one();
        return true;
    }

private:
    const ChannelId chId;
    std::mutex lck;
    std::condition_variable cv;
    std::queue<DataBuf> msgs;
    bool opened = true;
    bool closing = false;
    std::shared_ptr<IIo> io;
};
}
