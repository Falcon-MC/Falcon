#include "World/Level/LevelGen/Overworld/Feature/Tree/BambooJungleTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleBigTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *BambooJungleTreeFeature::NAME = "minecraft:bamboo_jungle_surface_trees_feature";

const char *BambooJungleTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr BambooJungleTreeFeature::getGenerator(IRandom &random) {
    (void) random;
    return std::make_shared<JungleBigTree>(10, 20);
}

bool BambooJungleTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isBamboo(biomeId);
}

int32_t BambooJungleTreeFeature::getMin() const {
    return -1;
}

int32_t BambooJungleTreeFeature::getMax() const {
    return 1;
}

bool BambooJungleTreeFeature::checkBlock(const BlockState &state, int32_t y) const {
    if (state.mName == "minecraft:bamboo")
        return false;

    return ObjectGeneratorFeature::checkBlock(state, y);
}
