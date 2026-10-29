#include "World/Level/LevelGen/Overworld/Feature/Tree/SavannaMutatedTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SavannaTree.h"

const char *SavannaMutatedTreeFeature::NAME = "minecraft:savanna_mutated_surface_trees_feature";

const char *SavannaMutatedTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr SavannaMutatedTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(3) == 0) {
        if (random.nextInt(100) == 0)
            return std::make_shared<FallenTree>();

        return std::make_shared<LegacyOakTree>();
    }

    return std::make_shared<SavannaTree>();
}
