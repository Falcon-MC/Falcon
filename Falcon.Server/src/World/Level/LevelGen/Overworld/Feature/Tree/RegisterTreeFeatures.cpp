#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/CherryTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/FlowerForestTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/GroveTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MeadowTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MesaPlateauStoneTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MesaTreeFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/MushroomIslandHugeMushroomFeature.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/PaleGardenTreeFeature.h"

void registerTreeFeatures() {
    GenerateFeatureRegistry::registerFeature(std::make_shared<CherryTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<FlowerForestTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<GroveTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MeadowTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MesaPlateauStoneTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MesaTreeFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<MushroomIslandHugeMushroomFeature>());
    GenerateFeatureRegistry::registerFeature(std::make_shared<PaleGardenTreeFeature>());
}
