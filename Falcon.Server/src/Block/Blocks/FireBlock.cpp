#include "Block/Blocks/FireBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(FireBlock, 326);

bool FireBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:fire" || identifier == "minecraft:soul_fire";
}

bool FireBlock::canBeReplaced(const BlockState &state) const {
    (void) state;

    return true;
}
