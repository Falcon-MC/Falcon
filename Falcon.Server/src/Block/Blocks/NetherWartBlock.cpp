#include "Block/Blocks/NetherWartBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(NetherWartBlock, 260);

bool NetherWartBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:nether_wart";
}
