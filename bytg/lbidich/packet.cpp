#include "packet.h"
#include <algorithm>

namespace  lbidich {

DataBuf packMsg(uint8_t id, const uint8_t *msg, unsigned size)
{
    const Header h = {id, (uint16_t)size};
    auto hptr = reinterpret_cast<const uint8_t*>(&h);
    DataBuf rv;
    std::copy(hptr, hptr+sizeof(h), std::back_inserter(rv));
    std::copy(msg, msg+h.dataSize, std::back_inserter(rv));
    return rv;
}

const uint8_t* PacketIn::load(const uint8_t* buf, unsigned int size)
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


}
