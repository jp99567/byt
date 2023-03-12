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

struct CanOutObjManager
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

void insertInputItem(can::CanInputItemsMap& inputsMap, can::Id canAddr, std::unique_ptr<can::IInputItem> item)
{
    std::vector<std::unique_ptr<can::IInputItem>> vec;
    auto rv = inputsMap.try_emplace(canAddr, std::move(vec));
    rv.first->second.emplace_back(std::move(item));
}

void Builder::buildCan(CanBus &canbus)
{
    auto node = config["NodeCAN"];
    std::vector<std::shared_ptr<CanNodeInfo>> nodes;
    std::vector<std::unique_ptr<IDigiOut>> digiOutputs;
    std::vector<std::reference_wrapper<DigInput>> digInputs;
    std::vector<std::reference_wrapper<SensorInput>> sensors;

    CanOutObjManager outObj;
    can::CanInputItemsMap inputsMap;

    auto outputControl = std::make_shared<can::OutputControl>(canbus);


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
                float convFactor = 16;
                if((it->second)["type"] && (it->second)["type"].as<std::string>() == "DS18S20")
                    convFactor = 2;
                auto sensor = std::make_unique<can::OwTempItem>(name, offset, convFactor);
                sensors.emplace_back(*sensor);
                insertInputItem(inputsMap, canAddr, std::move(sensor));
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
                auto input = std::make_unique<can::DigiInItem>(name, mask, offset);
                digInputs.emplace_back(*input);
                insertInputItem(inputsMap, canAddr, std::make_unique<can::DigiInItem>(name, mask, offset));
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
                item.busOutControl = outputControl;
                digiOutputs.emplace_back(std::move(digiOut));
            }
        }
    }

    outputControl->setOutputs(std::move(outObj.outputObjects));
    can::InputControl canInputControl(std::move(inputsMap));
    canbus.setOnRecv([&canInputControl](const can_frame& msg){
        canInputControl.onRecvMsg(msg);
    });

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
