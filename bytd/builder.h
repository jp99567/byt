#pragma once
#include <yaml-cpp/yaml.h>
#include "canbus.h"

class Builder
{
    const YAML::Node config;
public:
    Builder();
    void buildCan(CanBus& canbus);
    std::vector<std::tuple<std::string, std::string>> buildBBoW() const;
};
