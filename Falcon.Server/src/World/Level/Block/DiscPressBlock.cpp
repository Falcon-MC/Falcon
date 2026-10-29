#include "World/Level/Block/DiscPressBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(DiscPressBlock, 332);

bool DiscPressBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:disc_press";
}
