#include "Block/Blocks/CoralBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockData.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/CopperSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(CoralBlock, 185);

namespace {
    const std::string PREFIX = "minecraft:";
    const std::string DEAD_PREFIX = "minecraft:dead_";
    const int32_t MIN_DEATH_DELAY = 60;
    const int32_t DEATH_DELAY_SPREAD = 40;

    const Vector3i NEIGHBOUR_OFFSETS[] = {
            Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
            Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)
    };

    bool endsWith(const std::string &value, const std::string &suffix) {
        return value.size() >= suffix.size()
               && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool isCoralBlock(const std::string &identifier) {
        return endsWith(identifier, "_coral_block");
    }
}

bool CoralBlock::isLiveCoral(const std::string &identifier) {
    if (identifier.compare(0, PREFIX.size(), PREFIX) != 0 || identifier.compare(0, DEAD_PREFIX.size(), DEAD_PREFIX) == 0)
        return false;

    return endsWith(identifier, "_coral") || endsWith(identifier, "_coral_block")
           || endsWith(identifier, "_coral_fan") || endsWith(identifier, "_coral_wall_fan");
}

bool CoralBlock::matches(const std::string &identifier) {
    return isLiveCoral(identifier) && !endsWith(identifier, "_coral_fan") && !endsWith(identifier, "_coral_wall_fan");
}

void CoralBlock::scheduleDeathCheck(Level &level, const Vector3i &position) {
    level.scheduleUpdate(position, MIN_DEATH_DELAY + RandomTickSystem::nextInt(DEATH_DELAY_SPREAD));
}

bool CoralBlock::isWater(Level &level, const Vector3i &position, int layer) {
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z, layer);
    return state != nullptr && WaterBlock::matches(state->mName);
}

bool CoralBlock::hasWater(Level &level, const Vector3i &position, const BlockState &state) {
    if (!isCoralBlock(state.mName))
        return isWater(level, position, 1);

    for (const Vector3i &offset: NEIGHBOUR_OFFSETS) {
        const Vector3i neighbour(position.x + offset.x, position.y + offset.y, position.z + offset.z);
        if (isWater(level, neighbour, 0) || isWater(level, neighbour, 1))
            return true;
    }

    return false;
}

void CoralBlock::dieWithoutWater(Level &level, const Vector3i &position, const BlockState &state) {
    const BlockState current = level.getBlockState(position.x, position.y, position.z);
    if (current.mName != state.mName || !isLiveCoral(current.mName) || hasWater(level, position, current))
        return;

    const std::string dead = DEAD_PREFIX + current.mName.substr(PREFIX.size());
    if (BlockDataTable::find(dead.c_str()) == nullptr)
        return;

    level.setBlock(position, CopperSystem::transform(current, dead), true);
}

void CoralBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) const {
    (void) owner;

    dieWithoutWater(level, position, state);
}

void CoralBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) const {
    Block::onNeighbourChanged(owner, level, position, state);

    if (level.getBlockState(position.x, position.y, position.z).mName == state.mName)
        scheduleDeathCheck(level, position);
}
