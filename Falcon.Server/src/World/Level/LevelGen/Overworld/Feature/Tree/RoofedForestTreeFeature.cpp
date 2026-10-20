#include "World/Level/LevelGen/Overworld/Feature/Tree/RoofedForestTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/DarkOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

namespace {

    const ObjectGeneratorPtr &sharedGenerator() {
        static const ObjectGeneratorPtr generator = std::make_shared<DarkOakTree>();
        return generator;
    }

}

const char *RoofedForestTreeFeature::NAME = "minecraft:roofed_forest_tree_feature_rules";

const char *RoofedForestTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr RoofedForestTreeFeature::getGenerator(IRandom &random) {
    (void) random;
    return sharedGenerator();
}

int32_t RoofedForestTreeFeature::getMin() const {
    return 8;
}

int32_t RoofedForestTreeFeature::getMax() const {
    return 10;
}

bool RoofedForestTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isRoofed(biomeId);
}
