#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyOakTree.h"

#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/FancyOakTree.h"

TreeWoodType LegacyOakTree::getType() const {
    return TreeWoodType::OAK;
}

void LegacyOakTree::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    if (_hasFancyBrace(manager, x, y, z)) {
        FancyOakTree().generate(manager, random, x, y, z);
        return;
    }

    mTreeHeight = random.nextInt(3) + 4;
    LegacyTreeObject::placeObject(manager, x, y, z, random);
}

bool LegacyOakTree::canPlaceObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    return _hasFancyBrace(manager, x, y, z) || LegacyTreeObject::canPlaceObject(manager, x, y, z, random);
}

bool LegacyOakTree::_hasFancyBrace(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    for (int32_t xx = -1; xx <= 1; xx++) {
        for (int32_t zz = -1; zz <= 1; zz++) {
            if (xx == 0 && zz == 0)
                continue;

            if (!overridable(manager.getBlockAt(x + xx, y + 1, z + zz)))
                return true;
        }
    }

    return false;
}
