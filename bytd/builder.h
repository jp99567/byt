#pragma once
#include <yaml-cpp/yaml.h>

class Builder
{
    const YAML::Node config;
public:
    Builder();
    void buildCan();
    std::vector<std::tuple<std::string, std::string>> build() const;
};

