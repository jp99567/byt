#include "builder.h"
#include <memory>

#include "Log.h"
#include "candata.h"

Builder::Builder(std::shared_ptr<MqttClient> mqtt)
    : config(YAML::LoadFile("test1.yaml"))
    , mqtt(mqtt)
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
            return outputObjects.size()-1;
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

std::unique_ptr<can::InputControl> Builder::buildCan(CanBus &canbus)
{
    auto node = config["NodeCAN"];
    std::vector<std::shared_ptr<CanNodeInfo>> nodes;

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
                auto sensor = std::make_unique<can::OwTempItem>(name, offset, convFactor, mqtt);
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
                insertInputItem(inputsMap, canAddr, std::move(input));
            }
        }
        if(it->second["DigOUT"]){
            auto digiOutNode = it->second["DigOUT"];
            for(auto it = digiOutNode.begin(); it != digiOutNode.end(); ++it){
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
                digiOutputs.emplace(name, std::move(digiOut));
            }
        }
    }

    outputControl->setOutputs(std::move(outObj.outputObjects));
    auto canInputControl = std::make_unique<can::InputControl>(std::move(inputsMap));
    canbus.setOnRecv([p = canInputControl.get()](const can_frame& msg){
        p->onRecvMsg(msg);
    });
    return canInputControl;
}

ow::SensorList Builder::buildBBoW()
{
    auto node = config["BBOw"];
    ow::SensorList tsensors;
    for(auto it = node.begin(); it != node.end(); ++it){
        auto name = it->first.as<std::string>();
        auto romcode = (it->second)["owRomCode"].as<std::string>();
        LogINFO("yaml configured sensor {} {}", name, "a");
        tsensors.emplace_back(std::make_unique<ow::Sensor>(ow::RomCode(romcode), name));
        sensors.emplace_back(*tsensors.back());
    }
    return tsensors;
}

DigInput& getDigInputByName(std::vector<std::reference_wrapper<DigInput>>& digInputs, std::string name)
{
    for(auto& in : digInputs){
        if(in.get().name == name)
            return in;
    }
    throw std::runtime_error("getDigInputByName no such name");
}

std::unique_ptr<IDigiOut> getDigOutputByName(std::map<std::string, std::unique_ptr<IDigiOut>>& digiOutputs, std::string name)
{
    auto it = digiOutputs.find(name);
    if(it == std::cend(digiOutputs)){
        throw std::runtime_error(std::string("no such DigOutput ").append(name));
    }
    auto rv = std::move(it->second);
    digiOutputs.erase(it);
    return rv;
}


OnOffDeviceList Builder::buildOnOffDevices()
{
    buildDevice("SvetloKupelna", "svetloKupelna", "buttonKupelna1U");
    buildDevice("SvetloSpalna", "svetloSpalna", "buttonKupelna1D");
    buildDevice("SvetloChodbicka", "svetloChodbicka", "buttonKupelna2U");
    buildDevice("SvetloStol", "svetloStol", "buttonKupelna2D");
    buildDevice("SvetloStena", "svetloStena");
    buildDevice("SvetloObyvka", "svetloObyvka");

    return std::move(devicesOnOff);
}

void Builder::buildDevice(std::string name, std::string outputName, std::string inputName)
{
    auto out = getDigOutputByName(digiOutputs, outputName);
    auto it = devicesOnOff.emplace(std::string(OnOffDevice::mqttPathPrefix).append(name), std::make_unique<OnOffDevice>(std::move(out), name, mqtt));
    if(not inputName.empty()){
        auto& input = getDigInputByName(digInputs, inputName);
        input.Changed.subscribe(event::subscr([&dev=it.first->second](bool on){
            LogDBG("check {}", on);
            if(not on){
                dev->toggle();
            }
        }));
    }
}
