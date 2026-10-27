#include "World/Level/LevelGen/Overworld/Feature/Tree/MesaTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *MesaTreeFeature::NAME = "minecraft:mesa_tree_feature";

const char *MesaTreeFeature::name() const {
    return NAME;
}

TreeGeneratorPtr MesaTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(100) == 0)
        return std::make_shared<FallenTree>();

    return std::make_shared<LegacyOakTree>();
}

bool MesaTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isMesa(biomeId);
}
