#include "Block/Blocks/LeavesBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LeavesBlock, 290);

#include "Block/BlockIdentifier.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Types/ItemStack.h"

#include <string>
#include <unordered_set>

namespace {
    const int LEAVES_SEARCH_DISTANCE = 7;

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

    BlockActionHandler::destroyBlock(owner, level, position, state, true, ItemStack::air());
}
