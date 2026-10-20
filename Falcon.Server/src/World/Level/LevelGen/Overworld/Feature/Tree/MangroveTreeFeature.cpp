#include "World/Level/LevelGen/Overworld/Feature/Tree/MangroveTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/MangroveTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *MangroveTreeFeature::NAME = "minecraft:mangrove_swamp_mangrove_tree_feature";

const char *MangroveTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr MangroveTreeFeature::getGenerator(IRandom &random) {
    const std::shared_ptr<MangroveTree> tree = std::make_shared<MangroveTree>(random.nextFloat() > 0.15f);
    tree->setWithBeeNest(random.nextFloat() < 0.04f);
    return tree;
}

bool MangroveTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isMangroveSwamp(biomeId);
}

int32_t MangroveTreeFeature::getMin() const {
    return 12;
}

int32_t MangroveTreeFeature::getMax() const {
    return 15;
}
