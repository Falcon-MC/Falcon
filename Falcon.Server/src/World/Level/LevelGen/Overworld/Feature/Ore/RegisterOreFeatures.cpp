#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/InfestedOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/LegacyEmeraldOreFeature.h"

#include <memory>

void registerOreFeatures() {
    GenerateFeatureRegistry::registerFeature(std::make_shared<InfestedOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacyEmeraldOreFeature>());
}
