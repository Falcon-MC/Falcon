#include "Block/Blocks/RedstoneLampBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedstoneLampBlock, 430);

namespace {
    const char *REDSTONE_LAMP = "minecraft:redstone_lamp";
    const char *LIT_REDSTONE_LAMP = "minecraft:lit_redstone_lamp";
}

bool RedstoneLampBlock::matches(const std::string &identifier)
{
    return identifier == REDSTONE_LAMP || identifier == LIT_REDSTONE_LAMP;
}

bool RedstoneLampBlock::isLit() const
{
    return getIdentifier() == LIT_REDSTONE_LAMP;
}

BlockState RedstoneLampBlock::getLitState(const BlockState &state)
{
    return BlockState(LIT_REDSTONE_LAMP, state.mStates);
}

BlockState RedstoneLampBlock::getUnlitState(const BlockState &state)
{
    return BlockState(REDSTONE_LAMP, state.mStates);
}
