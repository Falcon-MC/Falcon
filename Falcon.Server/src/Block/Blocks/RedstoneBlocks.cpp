#include "Block/Blocks/RedstoneBlocks.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(TrappedChestBlock, 35);
FALCON_REGISTER_BLOCK(ObserverBlock, 155);
FALCON_REGISTER_BLOCK(RedstoneTorchBlock, 165);
FALCON_REGISTER_BLOCK(RedstoneBlock, 420);
FALCON_REGISTER_BLOCK(RedstoneLampBlock, 430);

namespace {
    const char *REDSTONE_BLOCK = "minecraft:redstone_block";
    const char *REDSTONE_LAMP = "minecraft:redstone_lamp";
    const char *LIT_REDSTONE_LAMP = "minecraft:lit_redstone_lamp";
    const char *REDSTONE_TORCH = "minecraft:redstone_torch";
    const char *UNLIT_REDSTONE_TORCH = "minecraft:unlit_redstone_torch";
    const char *OBSERVER = "minecraft:observer";
    const char *TRAPPED_CHEST = "minecraft:trapped_chest";
}

bool RedstoneBlock::matches(const std::string &identifier)
{
    return identifier == REDSTONE_BLOCK;
}

bool RedstoneBlock::isSignalSource() const
{
    return true;
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

bool ObserverBlock::matches(const std::string &identifier)
{
    return identifier == OBSERVER;
}

bool ObserverBlock::isSignalSource() const
{
    return true;
}

bool TrappedChestBlock::matches(const std::string &identifier)
{
    return identifier == TRAPPED_CHEST;
}

bool TrappedChestBlock::isSignalSource() const
{
    return true;
}
