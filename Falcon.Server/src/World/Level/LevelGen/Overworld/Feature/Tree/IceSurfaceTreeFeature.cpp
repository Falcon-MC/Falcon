#include "World/Level/LevelGen/Overworld/Feature/Tree/IceSurfaceTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SmallSpruceTree.h"

const char *IceSurfaceTreeFeature::NAME = "minecraft:ice_surface_trees_feature";

const char *IceSurfaceTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr IceSurfaceTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(100) == 0)
        return std::make_shared<FallenTree>(TreeWoodType::SPRUCE);

    return std::make_shared<SmallSpruceTree>();
}

int32_t IceSurfaceTreeFeature::getMin() const {
    return -20;
}

int32_t IceSurfaceTreeFeature::getMax() const {
    return 1;
}
