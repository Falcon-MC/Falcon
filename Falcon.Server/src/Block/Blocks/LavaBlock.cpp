#include "Block/Blocks/LavaBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LavaBlock, 325);

bool LavaBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:lava" || identifier == "minecraft:flowing_lava";
}
