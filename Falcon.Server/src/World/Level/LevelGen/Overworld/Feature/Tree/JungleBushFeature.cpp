#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleBushFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleBush.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *JungleBushFeature::NAME = "minecraft:jungle_bush";

const char *JungleBushFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr JungleBushFeature::getGenerator(IRandom &random) {
    (void) random;
    return std::make_shared<JungleBush>();
}

bool JungleBushFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isJungle(biomeId);
}
