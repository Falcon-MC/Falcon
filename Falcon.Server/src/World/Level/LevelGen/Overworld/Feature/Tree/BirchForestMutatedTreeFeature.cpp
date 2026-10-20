#include "World/Level/LevelGen/Overworld/Feature/Tree/BirchForestMutatedTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyBirchTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyTallBirchTree.h"

const char *BirchForestMutatedTreeFeature::NAME = "minecraft:legacy:birch_forest_mutated_tree_feature";

const char *BirchForestMutatedTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr BirchForestMutatedTreeFeature::getGenerator(IRandom &random) {
    const bool fallen = random.nextInt(100) == 0;

    if (random.nextBoolean()) {
        if (fallen)
            return std::make_shared<FallenTree>(TreeWoodType::BIRCH, 4, 10);

        return std::make_shared<LegacyTallBirchTree>();
    }

    if (fallen)
        return std::make_shared<FallenTree>(TreeWoodType::BIRCH);

    return std::make_shared<LegacyBirchTree>();
}
