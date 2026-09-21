#include "Block/Blocks/FrostedIceBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"

#include <cstdlib>

FALCON_REGISTER_BLOCK(FrostedIceBlock, 135);

namespace {
    const int MAX_AGE = 3;
    const int MELT_LIGHT_LEVEL = 11;
    const int MIN_MELT_DELAY = 20;
    const int MAX_MELT_DELAY = 40;

    const Vector3i NEIGHBOUR_OFFSETS[] = {
            Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
            Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)
    };

    Vector3i offset(const Vector3i &position, const Vector3i &delta) {
        return Vector3i(position.x + delta.x, position.y + delta.y, position.z + delta.z);
    }

    BlockState waterSource() {
        Tag states = Tag::ofCompound();
        states.putInt("liquid_depth", 0);
        return BlockState("minecraft:water", states);
    }
}

bool FrostedIceBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

int32_t FrostedIceBlock::nextMeltDelay() {
    return MIN_MELT_DELAY + std::rand() % (MAX_MELT_DELAY - MIN_MELT_DELAY);
}

void FrostedIceBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) const {
    (void) owner;

    if (RandomTickSystem::getFullLight(level, position) > MELT_LIGHT_LEVEL
        && (std::rand() % 3 == 0 || countFrostedNeighbours(level, position) < 4)) {
        slightlyMelt(level, position, state, true);
        return;
    }

    level.scheduleUpdate(position, nextMeltDelay());
}

void FrostedIceBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    (void) owner;
    (void) state;

    if (countFrostedNeighbours(level, position) < 2)
        level.setBlock(position, waterSource(), true);
}

int FrostedIceBlock::countFrostedNeighbours(Level &level, const Vector3i &position) {
    int neighbours = 0;

    for (const Vector3i &delta: NEIGHBOUR_OFFSETS) {
        const Vector3i side = offset(position, delta);
        if (matches(level.getBlockState(side.x, side.y, side.z).mName) && ++neighbours >= 4)
            return neighbours;
    }

    return neighbours;
}

void FrostedIceBlock::slightlyMelt(Level &level, const Vector3i &position, const BlockState &state, bool source) {
    const int age = state.mStates.getInt("age", 0);
    if (age < MAX_AGE) {
        Tag states = state.mStates;
        states.putInt("age", age + 1);
        level.setBlock(position, BlockState(IDENTIFIER, states), true);
        level.scheduleUpdate(position, nextMeltDelay());
        return;
    }

    level.setBlock(position, waterSource(), true);
    if (!source)
        return;

    for (const Vector3i &delta: NEIGHBOUR_OFFSETS) {
        const Vector3i side = offset(position, delta);
        const BlockState neighbour = level.getBlockState(side.x, side.y, side.z);
        if (matches(neighbour.mName))
            slightlyMelt(level, side, neighbour, false);
    }
}
