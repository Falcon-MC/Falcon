#include "Block/Blocks/DiscPressBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(DiscPressBlock, 332);

bool DiscPressBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:disc_press";
}
