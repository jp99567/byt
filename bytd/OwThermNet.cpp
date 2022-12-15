#include "OwThermNet.h"

#include "../pru/rpm_iface.h"
#include "Log.h"

namespace pru {

class Response
{
    const uint8_t* data;
    const std::size_t len;
    ResponseCode code = eRspError;

public:
    Response(const uint8_t* data, std::size_t len)
    :data(data)
    ,len(len)
    {}

bool check()
{
    if( len < sizeof(int32_t))
    {
        LogERR("pru invalid msg len:{}", len);
        return false;
    }

    auto i = *reinterpret_cast<const int32_t*>(data);
    code = (ResponseCode)i;

    switch(code)
    {
      case eOwBusFailure0:
      case eOwBusFailure1:
      case eOwSearchResult00:
      case eOwSearchResult0:
      case eOwSearchResult1:
      case eOwSearchResult11:
      case eOwReadBitsFailure:
      case eOwWriteBitsOk:
      case eOwWriteBitsFailure:
          if(len == sizeof(int32_t))
              return true;
          LogERR("pru msg unexpected size len:{} code:{}", len, code);
        break;
      case eOwPresenceOk:
      case eOwNoPresence:
      case eOwBusFailureTimeout:
          if( len == 2*sizeof(int32_t) )
              return true;
          LogERR("pru msg unexpected size len:{} code:{}", len, code);
        break;
      case eRspError:
          LogERR("pru error msg. len:{}", len);
        break;
      case eOwReadBitsOk:
          if( len > sizeof(int32_t) )
              return true;
          LogERR("pru msg unexpected size len:{} code:{}", len, code);
        break;
      default:
          LogERR("pru unknown msg: {}({})", code, len);
        break;
    }

    return false;
}

int32_t getParam() const
{
    if(len < 2*sizeof(int32_t))
        return -1;

    return reinterpret_cast<const int32_t*>(data)[1];
}

ResponseCode getCode() const
{
    return code;
}

std::string getCodeStr() const
{
    return getCodeStr(code);
}

static std::string getCodeStr(ResponseCode code)
{
    switch(code){
    case eRspError:  return "eRspError";
    case eOwPresenceOk:  return "eOwPresenceOk";
    case eOwBusFailure0:  return "eOwBusFailure0";
    case eOwBusFailure1:  return "eOwBusFailure1";
    case eOwNoPresence:  return "eOwNoPresence";
    case eOwBusFailureTimeout:  return "eOwBusFailureTimeout";
    case eOwReadBitsOk:  return "eOwReadBitsOk";
    case eOwReadBitsFailure:  return "eOwReadBitsFailure";
    case eOwWriteBitsOk:  return "eOwWriteBitsOk";
    case eOwWriteBitsFailure:  return "eOwWriteBitsFailure";
    default:
        return "???";
    }
}
};
}

namespace ow {

void OwThermNet::write_bits(const void* data, std::size_t bitlen, bool strong_pwr)
{
    msg.wr.h.code = strong_pwr ? pru::Commands::eCmdOwWritePower : pru::Commands::eCmdOwWrite;
    msg.wr.h.bitlen = bitlen;

    auto bytes = 1+(bitlen-1)/8;
    memcpy(msg.wr.data, data, bytes);
    pru->send((uint8_t*)&msg, sizeof(msg.wr.h)+bytes);

    auto buf = rxMsg->wait(std::chrono::milliseconds(100));
    if(buf.size() <= 0)
    {
        LogERR("pru ow read failure");
        return;
    }

    pru::Response rsp(buf.data(), buf.size());

    if(not rsp.check())
    {
        return;
    }

    switch(rsp.getCode())
    {
    case pru::ResponseCode::eOwWriteBitsOk:
        return;
    case pru::ResponseCode::eOwWriteBitsFailure:
        LogERR("OwThermNet::write_bits failure");
        break;
    default:
        LogERR("OwThermNet::write_bits unknown failure");
        break;
    }
}

bool OwThermNet::read_bits(std::size_t bitlen)
{
    msg.wr.h.code = pru::Commands::eCmdOwRead;
    msg.wr.h.bitlen = bitlen;

    pru->send((uint8_t*)&msg, sizeof(msg.wr.h));

    auto buf = rxMsg->wait(std::chrono::milliseconds(100));
    if(buf.size() <= 0)
    {
        LogERR("ow read_bits failure");
        return false;
    }

    pru::Response rsp(buf.data(), buf.size());

    if(not rsp.check())
    {
        return false;
    }

    switch(rsp.getCode())
    {
    case pru::ResponseCode::eOwReadBitsOk:
        std::copy(std::cbegin(buf), std::cend(buf), (uint8_t*)&msg.rd);
        return true;
    case pru::ResponseCode::eOwReadBitsFailure:
        LogERR("OwThermNet::read_bits failure");
        break;
    default:
        LogERR("OwThermNet::read_bits unknown failure");
        break;
    }
    return false;
}

int OwThermNet::search_triplet(bool branch_direction)
{
    int32_t cmd = branch_direction ? pru::Commands::eCmdOwSearchDir1 : pru::Commands::eCmdOwSearchDir0;
    pru->send((uint8_t*)&cmd, sizeof(cmd));

    auto buf = rxMsg->wait(std::chrono::milliseconds(100));
    if(buf.size() <= 0)
    {
        LogERR("ow read search_triplet failure");
        return 3;
    }

    pru::Response rsp(buf.data(), buf.size());

    if(not rsp.check())
    {
        LogERR("ow search_triplet failure invalid response");
        return 3;
    }

    int retval(3);
    switch(rsp.getCode())
    {
    case pru::ResponseCode::eOwSearchResult00:
        retval = 0;
        break;
    case pru::ResponseCode::eOwSearchResult0:
        retval = 1;
        break;
    case pru::ResponseCode::eOwSearchResult1:
        retval = 2;
        break;
    case pru::ResponseCode::eOwSearchResult11:
    default:
        retval = 3;
        break;
    }

    return retval;
}

void OwThermNet::write_simple_cmd(Cmd cmd, bool strongpower)
{
    write_bits(&cmd, 8, strongpower);
}

float OwThermNet::measure(const ow::RomCode& rc)
{
    ThermScratchpad res;

    if(read_scratchpad(rc, res)){
        float t = res.temperature / 16.0;

        if(t > 125 || t < -55){
            LogERR("out of range:{}", t);
            return std::numeric_limits<float>::quiet_NaN();
        }
        return t;
    }
    return std::numeric_limits<float>::quiet_NaN();
}

std::vector<ow::RomCode> OwThermNet::search()
{
    int last_branch(-1);
    struct RomCode rc;
    uint8_t* const ptr = reinterpret_cast<uint8_t*>(&rc);
    std::vector<ow::RomCode> tmp;

    do{
        if(!presence()){
            LogERR("no sensor on the bus");
            return tmp;
        }

        write_simple_cmd(Cmd::SEARCH);

        int last_zero(-1);
        for(int bitidx = 0; bitidx<(int)sizeof(rc)*8; ++bitidx){
            int byte = bitidx/8, bit = bitidx%8;

            bool dir(0);
            bool expext_b00(false);
            if(bitidx == last_branch){
                dir = 1;
                expext_b00 = true;
            }
            else if(bitidx < last_branch)
                dir = ptr[byte] & (1<<bit);

            int res = search_triplet(dir);
            if(expext_b00 && res != 0){
                LogERR("search failed, b00 expected");
                return tmp;
            }

            bool newval(dir);
            if(res==0){
                if(!newval){
                    last_zero = bitidx;
                }
            }
            else if(res == 3){
                LogERR("search failed, b11");
                return tmp;
            }
            else{
                newval = res == 1;
            }

            if(newval)
                ptr[byte] |= (1<<bit);
            else
                ptr[byte] &= ~(1<<bit);
        }
        last_branch = last_zero;

        if(check_crc(rc)){
            tmp.push_back(rc);
            LogDBG("search: found {}", (std::string)rc);
        }
        else{
            LogERR("search: crc error {}", (std::string)rc);
        }
    }
    while(last_branch >= 0);

    return tmp;
}

bool OwThermNet::convert()
{
    if(!presence()){
        LogERR("no presence");
        return false;
    }

    write_simple_cmd(Cmd::SKIP_ROM);
    write_simple_cmd(Cmd::CONVERT, true);
    return true;
}

bool OwThermNet::presence()
{
    int32_t cmd = pru::Commands::eCmdOwInit;
    pru->send((uint8_t*)&cmd, sizeof(cmd));

    auto buf = rxMsg->wait(std::chrono::milliseconds(100));

    if(buf.size() <= 0)
    {
        LogERR("ow read presence failure");
        return false;
    }

    pru::Response rsp(buf.data(), buf.size());

    if(not rsp.check())
    {
        return false;
    }

    bool retval(false);

    switch(rsp.getCode())
    {
    case pru::eOwPresenceOk:
        retval = true;
        break;
    default:
        LogERR("presence:{}", rsp.getCodeStr());
        break;
    }

    if(rsp.getParam() >= 0)
    {
        spdlog::get("ow")->trace("ow bus falling edge: {}us", 5e-3*rsp.getParam());
    }

    return retval;
}

bool OwThermNet::read_scratchpad(const RomCode& rc, ThermScratchpad& v)
{
    if(!presence()){
        LogERR("no presence {}", (std::string)rc);
        return false;
    }

    write_simple_cmd(Cmd::MATCH_ROM);
    write_bits(&rc, 8*sizeof(rc) );
    write_simple_cmd(Cmd::READ_SCRATCHPAD);
    if( not read_bits(8*sizeof(v)) )
        return false;

    memcpy(&v, msg.rd.data, sizeof(v));

    if( (v.conf != 0x7F ) ||
        (v.reserved[0]!=0xFF) ||
        (v.reserved[2]!=0x10) ){
        LogERR("ow scratchpad unexpected: {:02X} {:02X} {:02X} {:02X} {:02X}",
                (uint8_t)v.alarmH, (uint8_t)v.alarmL, v.conf, v.reserved[0], v.reserved[2] );
        return false;
    }

    if(!check_crc(v)){
        LogERR("ow scratchpad read rc={} crc error", (std::string)rc);
        return false;
    }

    return true;
}

bool OwThermNet::read_rom(RomCode& rc)
{
    if(!presence())
        return false;

    write_simple_cmd(Cmd::READ_ROM);

    auto buf = rxMsg->wait(std::chrono::milliseconds(100));
    if( buf.size() != sizeof(rc)) LogSYSDIE();

    std::copy(std::cbegin(buf), std::cend(buf), (uint8_t*)&rc);
    if(!check_crc(rc)){
        LogERR("ow read_rom error crc");
        return false;
    }
    return true;
}

OwThermNet::OwThermNet(std::shared_ptr<Pru> pru)
    :pru(pru)
    ,rxMsg(std::make_shared<PruRxMsg>())
{
    pru->setOwCbk(rxMsg);
}

OwThermNet::~OwThermNet()
{
    LogDBG("~OwThermNet");
}

} //namespace ow
