#include "World/Level/LevelGen/Overworld/Feature/Tree/SwampTreeFeature.h"

#include "World/Level/Block/BlockData.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SwampOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *SwampTreeFeature::NAME = "minecraft:swamp_oak_tree_feature";

const char *SwampTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr SwampTreeFeature::getGenerator(IRandom &random) {
    (void) random;
    return std::make_shared<SwampOakTree>(7, 8);
}

bool SwampTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isSwamp(biomeId);
}

int32_t SwampTreeFeature::getMin() const {
    return 3;
}

int32_t SwampTreeFeature::getMax() const {
    return 5;
}

bool SwampTreeFeature::checkBlock(const BlockState &state, int32_t y) const {
    (void) y;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return true;

    return !data->mSolid;
}
