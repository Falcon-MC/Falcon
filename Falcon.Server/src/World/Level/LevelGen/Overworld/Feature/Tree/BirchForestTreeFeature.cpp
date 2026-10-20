#include "World/Level/LevelGen/Overworld/Feature/Tree/BirchForestTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyBirchTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *BirchForestTreeFeature::NAME = "minecraft:birch_forest_surface_trees_feature";

const char *BirchForestTreeFeature::name() const {
    return NAME;
}

TreeGeneratorPtr BirchForestTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(100) == 0)
        return std::make_shared<FallenTree>(TreeWoodType::BIRCH);

    return std::make_shared<LegacyBirchTree>();
}

int32_t BirchForestTreeFeature::getMin() const {
    return 7;
}

int32_t BirchForestTreeFeature::getMax() const {
    return 8;
}

float BirchForestTreeFeature::getBeeNestChance() const {
    return 0.00035f;
}

bool BirchForestTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isBirch(biomeId);
}
