#include "World/Level/LevelGen/Overworld/Feature/Tree/LegacyJungleTree.h"

TreeWoodType LegacyJungleTree::getType() const {
    return TreeWoodType::JUNGLE;
}

void LegacyJungleTree::placeObject(BlockManager &manager, int32_t x, int32_t y, int32_t z, IRandom &random) {
    mTreeHeight = random.nextInt(6) + 4;
    LegacyTreeObject::placeObject(manager, x, y, z, random);
}
