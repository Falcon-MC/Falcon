#include "Level/Generator/Feature/Tree/VanillaTreeObject.h"

#include "Block/BlockData.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/LevelChunk.h"

namespace {

    const char *GROWABLE_BLOCKS[] = {
            "minecraft:air",
            "minecraft:acacia_leaves",
            "minecraft:azalea_leaves",
            "minecraft:birch_leaves",
            "minecraft:azalea_leaves_flowered",
            "minecraft:cherry_leaves",
            "minecraft:dark_oak_leaves",
            "minecraft:jungle_leaves",
            "minecraft:mangrove_leaves",
            "minecraft:oak_leaves",
            "minecraft:spruce_leaves",
            "minecraft:pale_oak_leaves",
            "minecraft:grass_block",
            "minecraft:dirt",
            "minecraft:acacia_log",
            "minecraft:birch_log",
            "minecraft:oak_log",
            "minecraft:pale_oak_log",
            "minecraft:dark_oak_log",
            "minecraft:jungle_log",
            "minecraft:mangrove_log",
            "minecraft:spruce_log",
            "minecraft:vine",
            "minecraft:dirt_with_roots",
            "minecraft:cherry_log",
            "minecraft:mangrove_roots",
            "minecraft:mangrove_propagule",
            "minecraft:acacia_sapling",
            "minecraft:cherry_sapling",
            "minecraft:spruce_sapling",
            "minecraft:bamboo_sapling",
            "minecraft:oak_sapling",
            "minecraft:jungle_sapling",
            "minecraft:dark_oak_sapling",
            "minecraft:pale_oak_sapling",
            "minecraft:birch_sapling",
            "minecraft:fern",
            "minecraft:short_grass",
            "minecraft:tall_grass",
            "minecraft:pale_hanging_moss",
            "minecraft:closed_eyeblossom",
            "minecraft:open_eyeblossom",
            "minecraft:leaf_litter",
            "minecraft:bamboo"
    };

    const char *LEAF_BLOCKS[] = {
            "minecraft:acacia_leaves",
            "minecraft:azalea_leaves",
            "minecraft:azalea_leaves_flowered",
            "minecraft:birch_leaves",
            "minecraft:cherry_leaves",
            "minecraft:dark_oak_leaves",
            "minecraft:jungle_leaves",
            "minecraft:mangrove_leaves",
            "minecraft:oak_leaves",
            "minecraft:pale_oak_leaves",
            "minecraft:spruce_leaves"
    };

}

bool VanillaTreeObject::canGrowInto(const std::string &identifier) {
    for (const char *candidate: GROWABLE_BLOCKS) {
        if (identifier == candidate)
            return true;
    }

    return false;
}

bool VanillaTreeObject::isLeaves(const std::string &identifier) {
    for (const char *candidate: LEAF_BLOCKS) {
        if (identifier == candidate)
            return true;
    }

    return false;
}

bool VanillaTreeObject::isSolid(const BlockState &state) {
    return BlockSupport::isSolid(state);
}

bool VanillaTreeObject::canBeReplaced(const BlockState &state) {
    return !isSolid(state);
}

bool VanillaTreeObject::isAir(const BlockState &state) {
    return state.mName == "minecraft:air";
}

BlockState VanillaTreeObject::withPillarAxis(BlockState state, const char *axis) {
    state.mStates.putString("pillar_axis", axis);
    return BlockState(state.mName, state.mStates);
}

BlockState VanillaTreeObject::withVineDirectionBits(BlockState state, int32_t bits) {
    state.mStates.putInt("vine_direction_bits", bits);
    return BlockState(state.mName, state.mStates);
}

BlockState VanillaTreeObject::getVineState(int32_t meta) {
    return withVineDirectionBits(VanillaBlocks::VINE().toBlockState(), meta);
}

int32_t VanillaTreeObject::getMinHeight() {
    return LevelChunk::MIN_Y;
}

int32_t VanillaTreeObject::getMaxHeight() {
    return LevelChunk::MAX_Y + 1;
}

void VanillaTreeObject::setDirtAt(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, VanillaBlocks::DIRT().toBlockState());
}

void VanillaTreeObject::addVinesAroundLog(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    _addVine(manager, x - 1, y, z, 8);
    _addVine(manager, x + 1, y, z, 2);
    _addVine(manager, x, y, z - 1, 1);
    _addVine(manager, x, y, z + 1, 4);
}

void VanillaTreeObject::_addVine(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t meta) {
    if (manager.getBlockAt(x, y, z).mName == "minecraft:air")
        manager.setBlockStateAt(x, y, z, getVineState(meta));
}
