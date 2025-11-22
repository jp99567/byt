#include "LightManager.h"
#include "Log.h"

namespace Light {

Manager::Manager(std::shared_ptr<DimmEvent> ev):dimmEv(ev) {}

DimmableSptr Manager::add(std::string name, std::unique_ptr<IPwmDev> pwmdev)
{
    auto light = std::make_shared<Dimmable>(dimmEv, std::move(pwmdev));
    dimmableList.emplace(std::make_pair(name, light));
    return light;
}

static float safeConvertF(const std::string& msg)
{
    float v = 0.0;
    try {
        v = std::stof(msg);
    } catch(std::exception& e) {
        LogERR("conversion error {}", msg);
    }
    return v;
}

void Manager::process(std::filesystem::path topic, std::string msg)
{
    if(topic.has_parent_path())
    {
        auto name = topic.parent_path().native();
        auto prop = topic.filename().native();
        LogINFO("Light Manager::process {} {} {}", name, prop, msg);
        auto svetloIt = dimmableList.find(name);
        if(svetloIt != std::cend(dimmableList))
        {
            if(prop == "target")
            {
                svetloIt->second->setTarget(safeConvertF(msg));
            }
            else if(prop == "ramp")
            {
                svetloIt->second->setFullRangeRampSeconds(FloatSeconds(safeConvertF(msg)));
            }
        }
    }
}
}
