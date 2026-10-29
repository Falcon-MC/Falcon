#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/AndesiteOreLowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/AndesiteOreUpperFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/CoalOreLowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/CoalOreMountainsFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/CoalOreUpperFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/CopperOreDripstoneCavesFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/CopperOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreBuriedFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreLargeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreSquareFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/DioriteOreLowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/DioriteOreUpperFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/DirtOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/EmeraldOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/GoldOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/GoldOreLowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/GoldOreMesaFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/GraniteOreLowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/GraniteOreUpperFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/GravelOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/InfestedOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreMiddleFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreSmallFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreUpperFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/LapisOreBuriedFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/LapisOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/LegacyEmeraldOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/RedstoneOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/RedstoneOreLowerFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Ore/TuffOreFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/River/ClayFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/River/GravelFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/River/SandFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Terrain/CanyonFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Terrain/CaveFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Terrain/ExtraUndergroundCaveFeature.h"

#include <memory>

void registerOreFeatures() {
    GenerateFeatureRegistry::registerFeature(std::make_shared<AndesiteOreLowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<AndesiteOreUpperFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CoalOreLowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CoalOreMountainsFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CoalOreUpperFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CopperOreDripstoneCavesFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CopperOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DiamondOreBuriedFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DiamondOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DiamondOreLargeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DiamondOreSquareFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DioriteOreLowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DioriteOreUpperFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<DirtOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<EmeraldOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GoldOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GoldOreLowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GoldOreMesaFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GraniteOreLowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GraniteOreUpperFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GravelOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<InfestedOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IronOreMiddleFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IronOreSmallFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<IronOreUpperFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LapisOreBuriedFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LapisOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<LegacyEmeraldOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<RedstoneOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<RedstoneOreLowerFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<TuffOreFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<ClayFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GravelFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SandFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CanyonFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CaveFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<ExtraUndergroundCaveFeature>());
}
