#include "World/Level/LevelGen/Overworld/Feature/Tree/FlowerForestTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyBirchTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *FlowerForestTreeFeature::NAME = "minecraft:flower_forest_surface_trees_feature";

const char *FlowerForestTreeFeature::name() const {
    return NAME;
}

TreeGeneratorPtr FlowerForestTreeFeature::getGenerator(IRandom &random) {
    const bool fallen = random.nextInt(100) == 0;

    if (random.nextInt(10) < 6)
        return std::make_shared<LegacyOakTree>();

    if (fallen)
        return std::make_shared<FallenTree>(TreeWoodType::BIRCH);

    return std::make_shared<LegacyBirchTree>();
}

int32_t FlowerForestTreeFeature::getMin() const {
    return 8;
}

int32_t FlowerForestTreeFeature::getMax() const {
    return 6;
}

float FlowerForestTreeFeature::getBeeNestChance() const {
    return 0.03f;
}

bool FlowerForestTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isFlowerForest(biomeId);
}
