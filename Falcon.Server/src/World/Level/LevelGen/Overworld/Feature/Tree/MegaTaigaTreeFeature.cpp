#include "World/Level/LevelGen/Overworld/Feature/Tree/MegaTaigaTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/BigSpruceTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SmallSpruceTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *MegaTaigaTreeFeature::NAME = "minecraft:mega_taiga_surface_trees_feature";
const char *MegaTaigaTreeFeature::ALIAS = "minecraft:scatter_taiga_plant_feature";

const char *MegaTaigaTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr MegaTaigaTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(5) < 2)
        return std::make_shared<BigSpruceTree>();

    return std::make_shared<SmallSpruceTree>();
}

bool MegaTaigaTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isTaiga(biomeId);
}

int32_t MegaTaigaTreeFeature::getDistanceToNextField() const {
    return 1;
}
