#include "World/Level/LevelGen/Overworld/Feature/Tree/MeadowTreeFeature.h"

#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *MeadowTreeFeature::NAME = "minecraft:meadow_surface_trees_feature";

const char *MeadowTreeFeature::name() const {
    return NAME;
}

float MeadowTreeFeature::getBeeNestChance() const {
    return 1.0f;
}

bool MeadowTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isMeadow(biomeId);
}
