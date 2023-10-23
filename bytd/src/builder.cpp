#include "builder.h"
#include <memory>

#include "Log.h"
#include "candata.h"
#include "Event.h"
#include "MqttDigiOut.h"
#include "DigiOutI2cExpander.h"

Builder::Builder(std::shared_ptr<MqttClient> mqtt)
    : config(YAML::LoadFile("config.yaml"))
    , mqtt(mqtt)
{
}

class PrevetranieReku : public IDigiOut
{
    Reku& reku;
public:
    explicit PrevetranieReku(Reku& reku): reku(reku){}
    operator bool() const override
    {
        return reku.FlowPercent == 100;
    }

    bool operator()(bool value) override
    {
        reku.FlowPercent = value ? 100 : 30;
        return value;
    }
};

void Builder::buildMisc(slowswi2cpwm &ioexpander)
{
    digiOutputs.emplace("dverePavlac", std::make_unique<DigiOutI2cExpander>(ioexpander, 2));
    components.reku = std::make_unique<Reku>("/dev/ttyO4");
    digiOutputs.emplace("prevetranie", std::make_unique<PrevetranieReku>(*components.reku));
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
                input->Changed.subscribe(event::subscr([name](bool v){
                    LogINFO("not used digInput {} {}", name, v);
                }));
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
    throw std::runtime_error(std::string("getDigInputByName no such name:").append(name));
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

void assignVypinavButton(VypinacDuo& vypinac, VypinacDuo::Button id, DigInput& input)
{
    input.Changed.unsubscribe();
    input.Changed.subscribe(event::subscr([&vypinac, id](bool on){
        vypinac.pressed(id, !on);
    }));
}

void assignVypinavButton(VypinacSingle& vypinac, VypinacSingle::Button id, DigInput& input)
{
    input.Changed.unsubscribe();
    input.Changed.subscribe(event::subscr([&vypinac, id](bool on){
        vypinac.pressed(id, !on);
    }));
}

void Builder::vypinace(boost::asio::io_service &io_context)
{
    /* provizorny bbb2 mqtt client */
    digiOutputs.emplace("svetloWc", std::make_unique<MqttDigiOut>(mqtt, "ha/svetla/wc"));
    digiOutputs.emplace("svetloKuchyna", std::make_unique<MqttDigiOut>(mqtt, "ha/svetla/kuchyna"));
    digiOutputs.emplace("svetloPavlac", std::make_unique<MqttDigiOut>(mqtt, "ha/svetla/pavlac"));
    digiOutputs.emplace("svetloIzba", std::make_unique<MqttDigiOut>(mqtt, "ha/svetla/izba"));
    digiOutputs.emplace("vetranie", std::make_unique<MqttDigiOut>(mqtt, "ha/reku/vetranie1"));
    /* treba vymenit za can node vo wc */

    auto vypinacKupelka = std::make_unique<VypinacDuoWithLongPress>(io_context);
    auto vypinacZadverie = std::make_unique<VypinacDuoWithLongPress>(io_context);
    auto vypinacKuchyna = std::make_unique<VypinacDuoSimple>();
    auto vypinacChodbicka = std::make_unique<VypinacDuoSimple>();
    auto vypinacIzba = std::make_unique<VypinacSingle>(io_context);
    auto vypinacSpalna = std::make_unique<VypinacSingle>(io_context);

    assignVypinavButton(*vypinacKupelka, VypinacDuo::Button::LU, getDigInputByName(digInputs, "buttonKupelna1U"));
    assignVypinavButton(*vypinacKupelka, VypinacDuo::Button::LD, getDigInputByName(digInputs, "buttonKupelna1D"));
    assignVypinavButton(*vypinacKupelka, VypinacDuo::Button::RU, getDigInputByName(digInputs, "buttonKupelna2U"));
    assignVypinavButton(*vypinacKupelka, VypinacDuo::Button::RD, getDigInputByName(digInputs, "buttonKupelna2D"));

    assignVypinavButton(*vypinacChodbicka, VypinacDuo::Button::LU, getDigInputByName(digInputs, "buttonChodbickaLU"));
    assignVypinavButton(*vypinacChodbicka, VypinacDuo::Button::LD, getDigInputByName(digInputs, "buttonChodbickaLD"));
    assignVypinavButton(*vypinacChodbicka, VypinacDuo::Button::RU, getDigInputByName(digInputs, "buttonChodbickaLU"));
    assignVypinavButton(*vypinacChodbicka, VypinacDuo::Button::RD, getDigInputByName(digInputs, "buttonChodbickaLD"));

    assignVypinavButton(*vypinacZadverie, VypinacDuo::Button::LU, getDigInputByName(digInputs, "buttonZadverieLU"));
    assignVypinavButton(*vypinacZadverie, VypinacDuo::Button::LD, getDigInputByName(digInputs, "buttonZadverieLD"));
    assignVypinavButton(*vypinacZadverie, VypinacDuo::Button::RU, getDigInputByName(digInputs, "buttonZadverieRU"));
    assignVypinavButton(*vypinacZadverie, VypinacDuo::Button::RD, getDigInputByName(digInputs, "buttonZadverieRD"));

    assignVypinavButton(*vypinacKuchyna, VypinacDuo::Button::LU, getDigInputByName(digInputs, "buttonKuchyna1LU"));
    assignVypinavButton(*vypinacKuchyna, VypinacDuo::Button::LD, getDigInputByName(digInputs, "buttonKuchyna1LD"));
    assignVypinavButton(*vypinacKuchyna, VypinacDuo::Button::RU, getDigInputByName(digInputs, "buttonKuchyna1RU"));
    assignVypinavButton(*vypinacKuchyna, VypinacDuo::Button::RD, getDigInputByName(digInputs, "buttonKuchyna1RD"));

    assignVypinavButton(*vypinacIzba, VypinacSingle::Button::U, getDigInputByName(digInputs, "buttonIzbaU"));
    assignVypinavButton(*vypinacIzba, VypinacSingle::Button::D, getDigInputByName(digInputs, "buttonIzbaD"));

    assignVypinavButton(*vypinacSpalna, VypinacSingle::Button::U, getDigInputByName(digInputs, "buttonSpalnaU"));
    assignVypinavButton(*vypinacSpalna, VypinacSingle::Button::D, getDigInputByName(digInputs, "buttonSpalnaD"));

    buildDevice("SvetloKupelna", "svetloKupelna", vypinacKupelka->ClickedLU);
    buildDevice("SvetloSpalna", "svetloSpalna", vypinacSpalna->ClickedU);
    auto& svetloChodbicka = buildDevice("SvetloChodbicka", "svetloChodbicka", vypinacKupelka->ClickedRU);
    vypinacChodbicka->ClickedRU.subscribe(event::subscr([&svetloChodbicka]{svetloChodbicka.toggle();}));
    buildDevice("SvetloStol", "svetloStol", vypinacChodbicka->ClickedRD);
    buildDevice("SvetloStena", "svetloStena", vypinacChodbicka->ClickedLD);
    buildDevice("SvetloObyvka", "svetloObyvka", vypinacChodbicka->ClickedLU);
    auto& svetloKuchyna = buildDevice("SvetloKuchyna", "svetloKuchyna", vypinacZadverie->ClickedLU);
    vypinacIzba->ClickedD.subscribe(event::subscr([&svetloKuchyna]{svetloKuchyna.toggle();}));
    vypinacKuchyna->ClickedLU.subscribe(event::subscr([&svetloKuchyna]{svetloKuchyna.toggle();}));
    auto& prevetranie = buildDevice("Vetranie", "prevetranie", vypinacZadverie->ClickedRD);
    buildDevice("SvetloIzba", "svetloIzba", vypinacIzba->ClickedU);
    buildDevice("SvetloPavlac", "svetloPavlac", vypinacZadverie->ClickedRU);
    buildDevice("SvetloWc", "svetloWc", vypinacZadverie->ClickedLD);

    vypinacKuchyna->ClickedRD.subscribe(event::subscr([&prevetranie]{prevetranie.toggle();}));

    for(auto& dev : components.devicesOnOff){
        vypinacKupelka->ClickedLongBothD.subscribe(event::subscr([&ref = *dev.second]{
            ref.set(false);
        }));
        vypinacZadverie->ClickedLongBothD.subscribe(event::subscr([&ref = *dev.second]{
            ref.set(false);
        }));
    }

    components.vypinaceDuo.emplace_back(std::move(vypinacKupelka));
    components.vypinaceDuo.emplace_back(std::move(vypinacZadverie));
    components.vypinaceDuo.emplace_back(std::move(vypinacKuchyna));
    components.vypinaceDuo.emplace_back(std::move(vypinacChodbicka));

    components.vypinaceSingle.emplace_back(std::move(vypinacIzba));
    components.vypinaceSingle.emplace_back(std::move(vypinacSpalna));

    components.brana = std::make_unique<MonoStableDev>(getDigOutputByName(digiOutputs, "brana"), "Brana", mqtt, io_context, 0.25);
    components.dverePavlac = std::make_unique<MonoStableDev>(getDigOutputByName(digiOutputs, "dverePavlac"), "DverePavlac", mqtt, io_context, 2);
}

Builder::AppComponents Builder::getComponents()
{
    components.reku = std::make_unique<Reku>("/dev/ttyO4");
    return std::move(components);
}

OnOffDevice& Builder::buildDevice(std::string name, std::string outputName, event::Event<>& controlEvent)
{
    auto out = getDigOutputByName(digiOutputs, outputName);
    auto it = components.devicesOnOff.emplace(name, std::make_unique<OnOffDevice>(std::move(out), name, mqtt));
    controlEvent.subscribe(event::subscr([&dev=it.first->second](){ dev->toggle(); }));
    return *(it.first->second);
}
