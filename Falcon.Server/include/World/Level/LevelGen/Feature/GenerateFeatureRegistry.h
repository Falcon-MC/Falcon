#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

#include <string>
#include <unordered_map>

class GenerateFeatureRegistry {
public:
    static void registerFeature(const IFeaturePtr &feature);

    static IFeaturePtr get(const std::string &key);

    static bool has(const std::string &key);

    static void initialize();

private:
    static std::unordered_map<std::string, IFeaturePtr> &_features();
};
