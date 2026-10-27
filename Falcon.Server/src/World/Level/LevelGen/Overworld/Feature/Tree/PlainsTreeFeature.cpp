#include "World/Level/LevelGen/Overworld/Feature/Tree/PlainsTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *PlainsTreeFeature::NAME = "minecraft:plains_surface_trees_feature";

const char *PlainsTreeFeature::name() const {
    return NAME;
}

TreeGeneratorPtr PlainsTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(20) < 1) {
        if (random.nextInt(100) == 0)
            return std::make_shared<FallenTree>();

        return std::make_shared<LegacyOakTree>();
    }

    return nullptr;
}

int32_t PlainsTreeFeature::getMin() const {
    return 1;
}

int32_t PlainsTreeFeature::getMax() const {
    return 1;
}

float PlainsTreeFeature::getBeeNestChance() const {
    return 0.05f;
}

bool PlainsTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isPlains(biomeId);
}
