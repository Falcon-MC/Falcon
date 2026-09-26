#include "Block/Blocks/NetherVinesBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(NetherVinesBlock, 185);

bool NetherVinesBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:weeping_vines" || identifier == "minecraft:twisting_vines";
}
