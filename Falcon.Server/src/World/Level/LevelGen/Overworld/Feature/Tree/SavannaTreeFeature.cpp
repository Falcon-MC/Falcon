#include "World/Level/LevelGen/Overworld/Feature/Tree/SavannaTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObjectWrapper.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SavannaTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *SavannaTreeFeature::NAME = "minecraft:savanna_surface_trees_feature";

const char *SavannaTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr SavannaTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(3) == 0)
        return std::make_shared<LegacyTreeObjectWrapper>(std::make_shared<LegacyOakTree>());

    return std::make_shared<SavannaTree>();
}

int32_t SavannaTreeFeature::getMin() const {
    return 2;
}

int32_t SavannaTreeFeature::getMax() const {
    return 4;
}

bool SavannaTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isSavanna(biomeId);
}
