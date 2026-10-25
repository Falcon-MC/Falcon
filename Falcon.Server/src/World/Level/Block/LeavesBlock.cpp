#include "World/Level/Block/LeavesBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LeavesBlock, 290);

#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Types/ItemStack.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {
    const int LEAVES_SEARCH_DISTANCE = 7;

    // One-in-N chances indexed by Fortune level, clamped to the last entry.
    const int32_t SAPLING_CHANCE[] = {20, 16, 12, 10, 10};
    const int32_t STICK_CHANCE[] = {50, 40, 30, 10, 10};
    const int32_t APPLE_CHANCE[] = {200, 180, 160, 120, 40};
    const int32_t MAX_FORTUNE_INDEX = 4;

    struct LeafLoot {
        const char *mSapling;
        bool mApples;
    };

    const std::unordered_map<std::string, LeafLoot> &leafLoot() {
        static const std::unordered_map<std::string, LeafLoot> loot = {
                {"minecraft:oak_leaves", {"minecraft:oak_sapling", true}},
                {"minecraft:dark_oak_leaves", {"minecraft:dark_oak_sapling", true}},
                {"minecraft:spruce_leaves", {"minecraft:spruce_sapling", false}},
                {"minecraft:birch_leaves", {"minecraft:birch_sapling", false}},
                {"minecraft:jungle_leaves", {"minecraft:jungle_sapling", false}},
                {"minecraft:acacia_leaves", {"minecraft:acacia_sapling", false}},
                {"minecraft:cherry_leaves", {"minecraft:cherry_sapling", false}},
                {"minecraft:pale_oak_leaves", {"minecraft:pale_oak_sapling", false}},
                {"minecraft:azalea_leaves", {"minecraft:azalea", false}},
                {"minecraft:azalea_leaves_flowered", {"minecraft:flowering_azalea", false}},
                {"minecraft:orange_poplar_leaves", {"minecraft:poplar_sapling", false}},
                {"minecraft:red_poplar_leaves", {"minecraft:poplar_sapling", false}},
                {"minecraft:yellow_poplar_leaves", {"minecraft:poplar_sapling", false}},
                {"minecraft:mangrove_leaves", {nullptr, false}}
        };
        return loot;
    }

    bool rollOneIn(const int32_t *chances, int32_t fortuneLevel) {
        return rand() % chances[std::min(std::max(fortuneLevel, 0), MAX_FORTUNE_INDEX)] == 0;
    }

    bool isLogState(const BlockState &state) {
        return BlockIdentifier::endsWith(state.mName, "_log")
               || BlockIdentifier::endsWith(state.mName, "_wood")
               || BlockIdentifier::endsWith(state.mName, "_stem")
               || BlockIdentifier::endsWith(state.mName, "_hyphae")
               || state.mName == "minecraft:mangrove_roots";
    }

    bool isLeavesState(const BlockState &state) {
        return BlockIdentifier::endsWith(state.mName, "_leaves");
    }

    bool findLog(Level &level, const Vector3i &position, int distance, std::unordered_set<int64_t> &visited) {
        static const int OFFSETS[6][3] = {
                {0, 1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
        };

        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        if (isLogState(state))
            return true;

        if (distance == 0 || !isLeavesState(state))
            return false;

        const int64_t key = BlockManager::hashXYZ(position.x, position.y, position.z) | (int64_t) distance;
        if (!visited.insert(key).second)
            return false;

        for (const auto &offset: OFFSETS) {
            const Vector3i side(position.x + offset[0], position.y + offset[1], position.z + offset[2]);
            if (findLog(level, side, distance - 1, visited))
                return true;
        }

        return false;
    }
}

bool LeavesBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_leaves");
}

PistonMoveReaction LeavesBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}

void LeavesBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    if (state.mStates.getByte("update_bit") != 0 || state.mStates.getByte("persistent_bit") != 0)
        return;

    Tag states = state.mStates;
    states.putByte("update_bit", 1);
    level.setBlock(position, BlockState(state.mName, states), false);
}

void LeavesBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    if (state.mStates.getByte("update_bit") == 0 || state.mStates.getByte("persistent_bit") != 0)
        return;

    std::unordered_set<int64_t> visited;
    if (findLog(level, position, LEAVES_SEARCH_DISTANCE, visited)) {
        Tag states = state.mStates;
        states.putByte("update_bit", 0);
        level.setBlock(position, BlockState(state.mName, states), false);
        return;
    }

    if (!BlockChangeSystem::allows(level, position, BlockState("minecraft:air"), BlockChangeCause::Decay))
        return;

    BlockActionHandler::destroyBlock(owner, level, position, state, true, ItemStack::air());
}

bool LeavesBlock::getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                           std::vector<BlockDrop> &drops) const {
    leafDrops(state, tool, fortuneLevel, drops);
    return true;
}

void LeavesBlock::leafDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                            std::vector<BlockDrop> &drops) {
    if (isShears(tool) || hasSilkTouch(tool)) {
        drops.push_back({state.mName, 1});
        return;
    }

    const auto loot = leafLoot().find(state.mName);
    if (loot == leafLoot().end())
        return;

    if (loot->second.mSapling != nullptr && rollOneIn(SAPLING_CHANCE, fortuneLevel))
        drops.push_back({loot->second.mSapling, 1});
    if (rollOneIn(STICK_CHANCE, fortuneLevel))
        drops.push_back({"minecraft:stick", 1});
    if (loot->second.mApples && rollOneIn(APPLE_CHANCE, fortuneLevel))
        drops.push_back({"minecraft:apple", 1});
}
