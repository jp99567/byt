#pragma once

#include <map>
#include <memory>
#include <filesystem>
#include "LightDimmable.h"

namespace Light {

class Manager
{
public:
    Manager(std::shared_ptr<DimmEvent> ev);
    DimmableSptr add(std::string name, std::unique_ptr<IPwmDev> pwmdev);
    void process(std::filesystem::path topic, std::string msg);

private:
    std::shared_ptr<DimmEvent> dimmEv;
    std::map<const std::string, DimmableSptr> dimmableList;
};

}
