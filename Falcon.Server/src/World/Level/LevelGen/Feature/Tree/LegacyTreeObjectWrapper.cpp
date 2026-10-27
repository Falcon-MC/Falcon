#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObjectWrapper.h"

LegacyTreeObjectWrapper::LegacyTreeObjectWrapper(const std::shared_ptr<LegacyTreeObject> &legacyTreeGenerator)
        : mLegacyTreeGenerator(legacyTreeGenerator) {
}

bool LegacyTreeObjectWrapper::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    if (mLegacyTreeGenerator != nullptr) {
        mLegacyTreeGenerator->placeObject(manager, x, y, z, random);
        return true;
    }

    return false;
}
