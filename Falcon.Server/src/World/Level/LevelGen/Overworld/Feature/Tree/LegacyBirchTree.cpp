#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyBirchTree.h"

TreeWoodType LegacyBirchTree::getType() const {
    return TreeWoodType::BIRCH;
}

void LegacyBirchTree::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    mTreeHeight = random.nextInt(2) + 5;
    LegacyTreeObject::placeObject(manager, x, y, z, random);
}
