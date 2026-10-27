#include "World/Level/Block/RedstoneBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedstoneBlock, 420);

namespace {
    const char *REDSTONE_BLOCK = "minecraft:redstone_block";
}

bool RedstoneBlock::matches(const std::string &identifier)
{
    return identifier == REDSTONE_BLOCK;
}

bool RedstoneBlock::isSignalSource() const
{
    return true;
}
