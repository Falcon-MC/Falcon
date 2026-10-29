#include "World/Level/LevelGen/Overworld/Feature/Tree/ForestTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyBirchTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *ForestTreeFeature::NAME = "minecraft:forest_surface_trees_feature";

const char *ForestTreeFeature::name() const {
    return NAME;
}

TreeGeneratorPtr ForestTreeFeature::getGenerator(IRandom &random) {
    const bool fallen = random.nextInt(100) == 0;

    if (random.nextInt(10) < 6) {
        if (fallen)
            return std::make_shared<FallenTree>();

        return std::make_shared<LegacyOakTree>();
    }

    if (fallen)
        return std::make_shared<FallenTree>(TreeWoodType::BIRCH);

    return std::make_shared<LegacyBirchTree>();
}

int32_t ForestTreeFeature::getMin() const {
    return 7;
}

int32_t ForestTreeFeature::getMax() const {
    return 8;
}

float ForestTreeFeature::getBeeNestChance() const {
    return 0.00035f;
}

bool ForestTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isForest(biomeId);
}
