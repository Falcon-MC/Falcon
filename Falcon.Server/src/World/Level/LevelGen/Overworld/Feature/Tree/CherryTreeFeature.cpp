#include "World/Level/LevelGen/Overworld/Feature/Tree/CherryTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/CherryTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *CherryTreeFeature::NAME = "minecraft:cherry_grove_after_surface_cherry_tree_feature_rules";

const char *CherryTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr CherryTreeFeature::getGenerator(IRandom &random) {
    (void) random;
    return std::make_shared<CherryTree>();
}

int32_t CherryTreeFeature::getMin() const {
    return 2;
}

int32_t CherryTreeFeature::getMax() const {
    return 4;
}

bool CherryTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isCherryGrove(biomeId);
}
