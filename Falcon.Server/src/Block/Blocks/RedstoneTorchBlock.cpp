#include "Block/Blocks/RedstoneTorchBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(RedstoneTorchBlock, 165);

namespace {
    const char *REDSTONE_TORCH = "minecraft:redstone_torch";
    const char *UNLIT_REDSTONE_TORCH = "minecraft:unlit_redstone_torch";
}

bool RedstoneTorchBlock::matches(const std::string &identifier)
{
    return identifier == REDSTONE_TORCH || identifier == UNLIT_REDSTONE_TORCH;
}

bool RedstoneTorchBlock::isSignalSource() const
{
    return isLit();
}

bool RedstoneTorchBlock::isLit() const
{
    return getIdentifier() == REDSTONE_TORCH;
}

BlockState RedstoneTorchBlock::getLitState(const BlockState &state)
{
    return BlockState(REDSTONE_TORCH, state.mStates);
}

BlockState RedstoneTorchBlock::getUnlitState(const BlockState &state)
{
    return BlockState(UNLIT_REDSTONE_TORCH, state.mStates);
}
