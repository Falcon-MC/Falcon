#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"

std::unordered_map<std::string, IFeaturePtr> &GenerateFeatureRegistry::_features() {
    static thread_local std::unordered_map<std::string, IFeaturePtr> features;
    return features;
}

void GenerateFeatureRegistry::registerFeature(const IFeaturePtr &feature) {
    if (feature == nullptr)
        return;

    _features()[feature->identifier()] = feature;

    const std::string featureName = feature->name();
    if (_features().find(featureName) == _features().end())
        _features()[featureName] = feature;
}

IFeaturePtr GenerateFeatureRegistry::get(const std::string &key) {
    const std::unordered_map<std::string, IFeaturePtr>::const_iterator found = _features().find(key);
    if (found == _features().end())
        return nullptr;

    return found->second;
}

bool GenerateFeatureRegistry::has(const std::string &key) {
    return _features().find(key) != _features().end();
}
