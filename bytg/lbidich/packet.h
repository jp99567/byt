#pragma once
#include <vector>
#include <cstdint>

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
    const uint8_t* load(const uint8_t* buf, unsigned size);

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

template<typename T>
DataBuf packMsg2(uint8_t id, const T& data)
{
    return packMsg(id, (uint8_t*)std::begin(data), sizeof(*std::begin(data))*(std::end(data)-std::begin(data)));
}

}
