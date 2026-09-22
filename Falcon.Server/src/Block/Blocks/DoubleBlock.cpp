#include "Block/Blocks/DoubleBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(DoublePlantBlock, 240);

#include "Block/BlockIdentifier.h"
#include "Level/Generator/Feature/IFeature.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"

namespace {
    const int32_t TALL_GRASS_SEED_CHANCE = 10;
}

bool DoublePlantBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                std::vector<BlockDrop> &drops) const {
    if (state.mName != "minecraft:tall_grass" && state.mName != "minecraft:large_fern")
        return false;

    drops = grassDrops(state, tool, fortuneLevel, TALL_GRASS_SEED_CHANCE);
    return true;
}

bool DoubleBlock::isUpperHalf(const BlockState &state) {
    return state.mStates.getByte("upper_block_bit") != 0;
}

std::vector<Vector3i> DoubleBlock::otherHalf(Level &level, const Vector3i &position, const BlockState &state) {
    if (!state.mStates.contains("upper_block_bit"))
        return {};

    const int32_t otherY = isUpperHalf(state) ? position.y - 1 : position.y + 1;
    if (otherY < level.getMinY() || otherY > level.getMaxY())
        return {};

    const Vector3i other(position.x, otherY, position.z);
    const BlockState otherState = level.getBlockState(other.x, other.y, other.z);

    if (otherState.mName != state.mName || isUpperHalf(otherState) == isUpperHalf(state))
        return {};

    return {other};
}

bool DoubleBlock::isComplete(Level &level, const Vector3i &position, const BlockState &state) {
    if (!state.mStates.contains("upper_block_bit"))
        return true;

    const int32_t otherY = isUpperHalf(state) ? position.y - 1 : position.y + 1;
    if (otherY < level.getMinY() || otherY > level.getMaxY())
        return false;

    const BlockState otherState = level.getBlockState(position.x, otherY, position.z);
    return otherState.mName == state.mName && isUpperHalf(otherState) != isUpperHalf(state);
}

bool DoublePlantBlock::matches(const std::string &identifier) {
    return BlockIdentifier::equalsAny(identifier, {
            "minecraft:tall_grass", "minecraft:large_fern", "minecraft:sunflower", "minecraft:lilac",
            "minecraft:rose_bush", "minecraft:peony", "minecraft:pitcher_plant"
    });
}

bool DoublePlantBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (position.y + 1 > level.getMaxY())
        return false;

    const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
    if (above.mName != "minecraft:air")
        return false;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    (void) blockFace;
    return IFeature::isSupportDirt(below);
}

bool DoublePlantBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    if (!DoubleBlock::isComplete(level, position, state))
        return false;

    if (DoubleBlock::isUpperHalf(state))
        return true;

    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    return IFeature::isSupportDirt(below);
}

std::vector<BlockPlacementEntry> DoublePlantBlock::getPlacementBlocks(Level &level, const Vector3i &position,
                                                                     const BlockState &state,
                                                                     int playerFacing) const {
    (void) level;
    (void) playerFacing;

    if (!state.mStates.contains("upper_block_bit") || DoubleBlock::isUpperHalf(state))
        return {};

    Tag states = state.mStates;
    states.putByte("upper_block_bit", 1);

    return {BlockPlacementEntry{Vector3i(position.x, position.y + 1, position.z),
                                BlockState(state.mName, states)}};
}

std::vector<Vector3i> DoublePlantBlock::getAffectedBlocks(Level &level, const Vector3i &position,
                                                          const BlockState &state) const {
    return DoubleBlock::otherHalf(level, position, state);
}
