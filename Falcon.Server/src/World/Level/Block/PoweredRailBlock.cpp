#include "World/Level/Block/PoweredRailBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

// before BaseRailBlock, which matches every rail
FALCON_REGISTER_BLOCK(PoweredRailBlock, 139);

namespace {
    const char *const STATE_DIRECTION = "rail_direction";

    bool isPowered(const BlockState &state) {
        return state.mStates.getByte(PoweredRailBlock::STATE_POWERED, 0) != 0;
    }

    int directionOf(const BlockState &state) {
        return state.mStates.getInt(STATE_DIRECTION, BaseRailBlock::STRAIGHT_NORTH_SOUTH);
    }

    bool isAscending(int direction) {
        return direction >= BaseRailBlock::ASCENDING_EAST && direction <= BaseRailBlock::ASCENDING_SOUTH;
    }
}

bool PoweredRailBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:golden_rail" || identifier == "minecraft:activator_rail";
}

void PoweredRailBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position)
                         || _findSignal(owner, level, position, state, true, 0)
                         || _findSignal(owner, level, position, state, false, 0);
    if (powered == isPowered(state))
        return;

    Tag states = state.mStates;
    states.putByte(STATE_POWERED, powered ? 1 : 0);
    level.setBlock(position, BlockState(state.mName, states), true);
    // the next rails of the line hear the change through the block under them, and above them on a slope
    RedstoneSystem::updateAroundRedstone(owner, level, Vector3i(position.x, position.y - 1, position.z));
    if (isAscending(directionOf(state)))
        RedstoneSystem::updateAroundRedstone(owner, level, Vector3i(position.x, position.y + 1, position.z));
}

// the next rail along the line, one step forward or back; a slope also looks one block lower
bool PoweredRailBlock::_findSignal(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state, bool forward, int distance) {
    if (distance >= MAX_DISTANCE)
        return false;

    int x = position.x;
    int y = position.y;
    int z = position.z;
    bool lower = true;
    int direction = directionOf(state);
    switch (direction) {
        case STRAIGHT_NORTH_SOUTH:
            z += forward ? 1 : -1;
            break;
        case STRAIGHT_EAST_WEST:
            x += forward ? -1 : 1;
            break;
        case ASCENDING_EAST:
            if (forward) {
                --x;
                ++y;
                lower = false;
            } else {
                ++x;
            }
            direction = STRAIGHT_EAST_WEST;
            break;
        case ASCENDING_WEST:
            if (forward) {
                --x;
            } else {
                ++x;
                ++y;
                lower = false;
            }
            direction = STRAIGHT_EAST_WEST;
            break;
        case ASCENDING_NORTH:
            if (forward) {
                ++z;
            } else {
                --z;
                ++y;
                lower = false;
            }
            direction = STRAIGHT_NORTH_SOUTH;
            break;
        case ASCENDING_SOUTH:
            if (forward) {
                ++z;
                ++y;
                lower = false;
            } else {
                --z;
            }
            direction = STRAIGHT_NORTH_SOUTH;
            break;
        default:
            return false;
    }

    if (_isPoweredRail(owner, level, Vector3i(x, y, z), state.mName, forward, distance, direction))
        return true;
    return lower && _isPoweredRail(owner, level, Vector3i(x, y - 1, z), state.mName, forward, distance, direction);
}

bool PoweredRailBlock::_isPoweredRail(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const std::string &name, bool forward, int distance, int direction) {
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    if (state == nullptr || state->mName != name)
        return false;

    const int other = directionOf(*state);
    if (direction == STRAIGHT_EAST_WEST
        && (other == STRAIGHT_NORTH_SOUTH || other == ASCENDING_NORTH || other == ASCENDING_SOUTH))
        return false;
    if (direction == STRAIGHT_NORTH_SOUTH
        && (other == STRAIGHT_EAST_WEST || other == ASCENDING_EAST || other == ASCENDING_WEST))
        return false;
    if (!isPowered(*state))
        return false;

    return RedstoneSystem::isGettingPower(owner, level, position)
           || _findSignal(owner, level, position, *state, forward, distance + 1);
}
