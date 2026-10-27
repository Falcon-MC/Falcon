#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyTallBirchTree.h"

TreeWoodType LegacyTallBirchTree::getType() const {
    return TreeWoodType::BIRCH;
}

void LegacyTallBirchTree::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    mTreeHeight = random.nextInt(3) + 10;
    LegacyTreeObject::placeObject(manager, x, y, z, random);
}
