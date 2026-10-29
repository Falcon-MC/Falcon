#include "World/Level/Block/DoublePlantBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/DoubleBlock.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"

FALCON_REGISTER_BLOCK(DoublePlantBlock, 240);

bool DoublePlantBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                std::vector<BlockDrop> &drops) const {
    (void) fortuneLevel;

    if (state.mName == "minecraft:tall_grass") {
        drops = grassDrops("minecraft:short_grass", 2, tool);
        return true;
    }

    if (state.mName == "minecraft:large_fern") {
        drops = grassDrops("minecraft:fern", 2, tool);
        return true;
    }

    return false;
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
