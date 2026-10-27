#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/FancyOakTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleBigTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/JungleTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *JungleTreeFeature::NAME = "minecraft:jungle_surface_trees_feature";

const char *JungleTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr JungleTreeFeature::getGenerator(IRandom &random) {
    const int32_t choice = random.nextInt(10);

    if (choice == 0)
        return std::make_shared<JungleBigTree>(10, 20);

    if (choice == 4 || choice == 5 || choice == 6) {
        if (random.nextInt(100) == 0)
            return std::make_shared<FallenTree>(TreeWoodType::JUNGLE);

        return std::make_shared<JungleTree>(4 + random.nextBoundedInt(7), 3);
    }

    if (choice == 7 || choice == 8)
        return std::make_shared<FancyOakTree>();

    if (random.nextInt(100) == 0)
        return std::make_shared<FallenTree>(TreeWoodType::JUNGLE);

    return std::make_shared<JungleTree>(7, 8);
}

bool JungleTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isJungle(biomeId);
}
