#include "World/Level/LevelGen/Overworld/Feature/Tree/MushroomIslandHugeMushroomFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/HugeMushroom.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *MushroomIslandHugeMushroomFeature::NAME = "minecraft:mushroom_island_surface_huge_mushroom_feature";

const char *MushroomIslandHugeMushroomFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr MushroomIslandHugeMushroomFeature::getGenerator(IRandom &random) {
    (void) random;
    return std::make_shared<HugeMushroom>();
}

int32_t MushroomIslandHugeMushroomFeature::getMin() const {
    return 1;
}

int32_t MushroomIslandHugeMushroomFeature::getMax() const {
    return 2;
}

bool MushroomIslandHugeMushroomFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isMooshroomIsland(biomeId);
}
