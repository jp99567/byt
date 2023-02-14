#include "builder.h"
#include <memory>

#include "Log.h"
#include "candata.h"

Builder::Builder() : config(YAML::LoadFile("config.yaml"))
{
}

struct CanNodeInfo
{
    explicit CanNodeInfo(std::string name, unsigned id) :name(name), id(id)
    {
        LogINFO("Node can {} {}", name, id);
    }
    std::string name;
    unsigned id;
};

struct OutObjManager
{
    std::vector<can::OutputMsg> outputObjects;

    std::size_t addCanObj(can::Id addr, std::size_t maxsize)
    {
        auto it = std::find_if(std::begin(outputObjects), std::end(outputObjects), [addr](can::OutputMsg& obj){
            return obj.msg.addr() == addr;
        });

        if(it == std::cend(outputObjects)){
            outputObjects.emplace_back(addr, maxsize);
            return outputObjects.size();
        }
        else{
            if(it->msg.frame.can_dlc < maxsize)
                it->msg.setSize(maxsize);
            return it-std::cbegin(outputObjects);
        }
    }
};

void Builder::buildCan()
{
    auto node = config["NodeCAN"];
    std::vector<std::shared_ptr<CanNodeInfo>> nodes;
    std::vector<std::unique_ptr<IDigiOut>> digiOutputs;

    OutObjManager outObj;


    for(auto it = node.begin(); it != node.end(); ++it){
        auto nodeName = it->first.as<std::string>();
        auto nodeId = (it->second)["id"].as<unsigned>();
        LogINFO("yaml configured node {} {}", nodeName, nodeId);
        nodes.emplace_back(std::make_shared<CanNodeInfo>(nodeName, nodeId));
        if(it->second["OwT"]){
            auto owts = it->second["OwT"];
            for(auto it = owts.begin(); it != owts.end(); ++it){
                auto name = it->first.as<std::string>();
                auto canAddr = (it->second)["addr"][0].as<can::Id>();
                auto offset = (it->second)["addr"][1].as<unsigned>();
                LogINFO("ow sensor {} {:X} {}", name, canAddr, offset);
            }
        }
        if(it->second["DigIN"]){
            auto digiIn = it->second["DigIN"];
            for(auto it = digiIn.begin(); it != digiIn.end(); ++it){
                auto name = it->first.as<std::string>();
                auto canAddr = (it->second)["addr"][0].as<unsigned>();
                auto offset = (it->second)["addr"][1].as<unsigned>();
                auto mask = 1 << (it->second)["addr"][2].as<unsigned>();
                LogINFO("dig IN {} {:X} {}, {:08b}", name, canAddr, offset, mask);
            }
        }
        if(it->second["DigOUT"]){
            auto digiIn = it->second["DigOUT"];
            for(auto it = digiIn.begin(); it != digiIn.end(); ++it){
                auto name = it->first.as<std::string>();
                auto canAddr = (it->second)["addr"][0].as<unsigned>();
                auto offset = (it->second)["addr"][1].as<unsigned>();
                auto mask = 1 << (it->second)["addr"][2].as<unsigned>();
                LogINFO("dig OUT {} {:X} {}, {:08b}", name, canAddr, offset, mask);
                auto idx = outObj.addCanObj(canAddr, offset+1);
                auto digiOut = std::make_unique<CanDigiOut>();
                auto& item = digiOut->getCanItem();
                item.offset = offset;
                item.mask = mask;
                item.idx = idx;
                item.busOutControl = nullptr;//ToDo
                digiOutputs.emplace_back(std::move(digiOut));
            }
        }
    }

}

std::vector<std::tuple<std::string, std::string>> Builder::buildBBoW() const
{
    auto node = config["BBOw"];
    std::vector<std::tuple<std::string, std::string>> tsensors;
    for(auto it = node.begin(); it != node.end(); ++it){
        auto name = it->first.as<std::string>();
        auto romcode = (it->second)["owRomCode"].as<std::string>();
        LogINFO("yaml configured sensor {} {}", name, "a");
        tsensors.emplace_back(std::make_tuple(name, romcode));
    }
    return tsensors;
}
