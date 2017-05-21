#pragma once

#include <vector>
#include <algorithm>

namespace  lbidich {

struct Header {
    uint8_t chId;
    uint16_t dataSize;
} __attribute__((packed));

using DataBuf = std::vector<uint8_t>;

struct Packet
{
    Header header;
    DataBuf msg;
};

class PacketIn : protected Packet
{
    unsigned header_left = sizeof(header);
    unsigned data_left = 0;
    uint8_t* header_ptr = reinterpret_cast<uint8_t*>(&header);

public:
    const uint8_t* load(const uint8_t* buf, unsigned size)
    {
        auto in_left = size;
        if(header_left)
        {
            auto n = std::min(header_left, in_left);
            header_ptr = std::copy(buf, buf+n, header_ptr);
            header_left -= n;
            buf += n;
            in_left -= n;
            if(header_left)
            {
                return nullptr;
            }
            else
            {
                data_left = header.dataSize;
                if(!data_left)
                    return buf;
            }
        }

        if(in_left > 0)
        {
            if(in_left < data_left)
            {
                std::copy(buf, buf+in_left, std::back_inserter(msg));
                data_left -= in_left;
                return nullptr;
            }
            else
            {
                std::copy(buf, buf+data_left, std::back_inserter(msg));
                return buf+data_left;
            }
        }

        return nullptr;
    }

    const Header& getHeader() const
    {
    	return header;
    }

    DataBuf getMsg()
    {
        header_left = sizeof(header);
        header_ptr = reinterpret_cast<uint8_t*>(&header);
        return std::move(msg);
    }
};

DataBuf packMsg(uint8_t id, const uint8_t* msg, unsigned size);

}
