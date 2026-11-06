#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/BambooJungleTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/BirchForestMutatedTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/BirchForestTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/CherryTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/FlowerForestTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/GroveTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleBushFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleEdgeTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MangroveTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MeadowTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MesaPlateauStoneTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MesaTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MushroomIslandHugeMushroomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/PaleGardenTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/RoofedForestTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SavannaMutatedTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SavannaTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SwampTreeFeature.h"

void registerTreeFeatures() {
    GenerateFeatureRegistry::registerFeature(std::make_shared<BambooJungleTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<BirchForestMutatedTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<BirchForestTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<CherryTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<FlowerForestTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GroveTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<JungleBushFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<JungleEdgeTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<JungleTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MangroveTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MeadowTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MesaPlateauStoneTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MesaTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MushroomIslandHugeMushroomFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<PaleGardenTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<RoofedForestTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SavannaMutatedTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SavannaTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<SwampTreeFeature>());
}
