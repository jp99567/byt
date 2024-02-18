#include "builder.h"
#include <memory>

#include "Log.h"
#include "candata.h"
#include "Event.h"
#include "MqttDigiOut.h"
#include "DigiOutI2cExpander.h"
#include "BBDigiOut.h"
#include "kurenieHW.h"

Builder::Builder(IMqttPublisherSPtr mqtt)
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

void Builder::buildMisc(slowswi2cpwm &ioexpander, OpenTherm &ot)
{
    digiOutputs.emplace("dverePavlac", std::make_unique<DigiOutI2cExpander>(ioexpander, 2));
    components.reku = std::make_unique<Reku>(mqtt, "/dev/ttyO4");
    digiOutputs.emplace("prevetranie", std::make_unique<PrevetranieReku>(*components.reku));
    std::unique_ptr<IDigiOut> pumpaDigOut;
#ifdef BYTD_SIMULATOR
    pumpaDigOut = std::make_unique<MqttDigiOut>(mqtt, "test/pumpa");
#else
    components.gpiochip3 = std::make_unique<gpiod::chip>("3");
    pumpaDigOut = std::make_unique<BBDigiOut>(*components.gpiochip3, 21);
#endif
    components.pumpa = std::make_unique<Pumpa>(std::move(pumpaDigOut), mqtt);

    kurenie::HW::SensorsT sensTrooms;
    auto ss = std::make_unique<SimpleSensor>("dummyKupelka", mqtt);
    ss->setValue(20);
    components.dummyKupelnaT = std::move(ss);
    auto assignSensor = [&sensTrooms](kurenie::ROOM room, ISensorInput& sens){
      if((std::size_t)room != sensTrooms.size())
        throw std::runtime_error("sensorT kurenie vytvoreny mimo poradia");
      sensTrooms.emplace_back(sens);
    };

    assignSensor(kurenie::ROOM::Obyvka, findSensor("tObyvka"));
    assignSensor(kurenie::ROOM::Spalna, findSensor("tSpalna"));
    assignSensor(kurenie::ROOM::Kuchyna, findSensor("tZadverie"));
    assignSensor(kurenie::ROOM::Izba, findSensor("tIzba"));
    assignSensor(kurenie::ROOM::Kupelka, *components.dummyKupelnaT);
    assignSensor(kurenie::ROOM::Podlahovka, findSensor("tPodlahovka"));

    kurenie::HW::SensorsT sensTpodlaha_privod;
    sensTpodlaha_privod.emplace_back(findSensor("tretKupelna"));
    sensTpodlaha_privod.emplace_back(findSensor("tretKupelnaP"));
    auto kurenieHw = std::make_unique<kurenie::HW>(ot, ioexpander, mqtt, std::move(sensTrooms), std::move(sensTpodlaha_privod));
    components.kurenie = std::make_unique<kurenie::Kurenie>(std::move(kurenieHw));
    components.ventil = std::make_unique<Ventil4w>([&ioexpander](bool on){
      ioexpander.dig1Out(on);
    }, mqtt);
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
                auto sensor = std::make_unique<can::OwTempItem>(name, offset, mqtt, convFactor);
                sensors.emplace_back(sensor->sens);
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
        LogINFO("yaml configured sensor BBow {}", name);
        tsensors.emplace_back(std::make_pair(ow::RomCode(romcode), std::make_unique<ow::Sensor>(name, mqtt)));
        sensors.emplace_back(*tsensors.back().second);
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
    auto& svetloSpalna = buildDevice("SvetloSpalna", "svetloSpalna", vypinacSpalna->ClickedU);
    auto& svetloChodbicka = buildDevice("SvetloChodbicka", "svetloChodbicka", vypinacKupelka->ClickedRU);
    vypinacChodbicka->ClickedRU.subscribe(event::subscr([&svetloChodbicka]{svetloChodbicka.toggle();}));
    buildDevice("SvetloStol", "svetloStol", vypinacChodbicka->ClickedRD);
    buildDevice("SvetloStena", "svetloStena", vypinacChodbicka->ClickedLD);
    buildDevice("SvetloObyvka", "svetloObyvka", vypinacChodbicka->ClickedLU);
    auto& svetloKuchyna = buildDeviceInvertedOut("SvetloKuchyna", "svetloKuchyna", vypinacZadverie->ClickedLU);
    vypinacIzba->ClickedD.subscribe(event::subscr([&svetloKuchyna]{svetloKuchyna.toggle();}));
    vypinacKuchyna->ClickedLU.subscribe(event::subscr([&svetloKuchyna]{svetloKuchyna.toggle();}));
    auto& prevetranie = buildDevice("Vetranie", "prevetranie", vypinacZadverie->ClickedRD);
    buildDeviceInvertedOut("SvetloIzba", "svetloIzba", vypinacIzba->ClickedU);
    buildDeviceInvertedOut("SvetloPavlac", "svetloPavlac", vypinacZadverie->ClickedRU);
    buildDeviceInvertedOut("SvetloWc", "svetloWc", vypinacZadverie->ClickedLD);

    vypinacKuchyna->ClickedRD.subscribe(event::subscr([&prevetranie]{prevetranie.toggle();}));
    vypinacSpalna->ClickedD.subscribe(event::subscr([&dev = svetloChodbicka]{dev.toggle();}));

    for(auto& dev : components.devicesOnOff){
        if(dev.second.get() != &svetloSpalna)
            vypinacSpalna->ClickedLongU.subscribe(event::subscr([&ref = *dev.second]{
                ref.set(false);
            }));
        else
            vypinacSpalna->ClickedLongU.subscribe(event::subscr([&ref = *dev.second]{
                ref.set(true);
            }));
        vypinacSpalna->ClickedLongD.subscribe(event::subscr([&ref = *dev.second]{
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

AppComponents Builder::getComponents()
{
    return std::move(components);
}

OnOffDevice &Builder::buildDevice(std::string name, std::string outputName,
                                  event::Event<> &controlEvent,
                                  bool outInverted)
{
    auto out = getDigOutputByName(digiOutputs, outputName);
    auto it = components.devicesOnOff.emplace(name, std::make_unique<OnOffDevice>(std::move(out), name, mqtt, outInverted));
    controlEvent.subscribe(event::subscr([&dev=it.first->second](){ dev->toggle(); }));
    return *(it.first->second);
}

ISensorInput &Builder::findSensor(std::string name)
{
  auto rv = std::find_if(std::cbegin(sensors), std::cend(sensors), [&name](auto item){
    return item.get().name() == name;
  });
  if(rv == std::cend(sensors))
    throw std::runtime_error(std::string("sensor not found ").append(name));
  return *rv;
}
