#include "Block/Blocks/TrappedChestBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(TrappedChestBlock, 35);

namespace {
    const char *TRAPPED_CHEST = "minecraft:trapped_chest";
}

bool TrappedChestBlock::matches(const std::string &identifier)
{
    return identifier == TRAPPED_CHEST;
}

bool TrappedChestBlock::isSignalSource() const
{
    return true;
}
