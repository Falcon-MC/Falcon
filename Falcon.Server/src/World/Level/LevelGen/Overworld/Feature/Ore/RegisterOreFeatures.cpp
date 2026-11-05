#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/InfestedOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/LegacyEmeraldOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/River/ClayFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/River/GravelFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/River/SandFeature.h"

#include <memory>

void registerOreFeatures() {
    GenerateFeatureRegistry::registerFeature(std::make_shared<InfestedOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacyEmeraldOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<ClayFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GravelFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SandFeature>());
}
