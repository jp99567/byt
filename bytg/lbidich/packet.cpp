#include "packet.h"

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

}
