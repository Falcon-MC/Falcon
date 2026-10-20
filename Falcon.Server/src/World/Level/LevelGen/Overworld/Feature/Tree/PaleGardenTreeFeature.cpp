#include "World/Level/LevelGen/Overworld/Feature/Tree/PaleGardenTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/PaleOakTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

const char *PaleGardenTreeFeature::NAME = "minecraft:random_pale_oak_tree_feature";

const char *PaleGardenTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr PaleGardenTreeFeature::getGenerator(IRandom &random) {
    const std::shared_ptr<PaleOakTree> object = std::make_shared<PaleOakTree>();
    object->mTryCreakingHeart = random.nextInt(4) == 0;
    return object;
}

int32_t PaleGardenTreeFeature::getMin() const {
    return 8;
}

int32_t PaleGardenTreeFeature::getMax() const {
    return 10;
}

bool PaleGardenTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isPaleGarden(biomeId);
}
