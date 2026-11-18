#include "World/Level/Block/BigDripleafBlock.h"

#include "Core/Math/VectorMath.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Actor.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cstdlib>

FALCON_REGISTER_BLOCK(BigDripleafBlock, 100);

namespace {
    const char *const STATE_HEAD = "big_dripleaf_head";
    const char *const STATE_TILT = "big_dripleaf_tilt";
    const char *const STATE_DIRECTION = "minecraft:cardinal_direction";

    const char *const TILT_NAMES[] = {"none", "unstable", "partial_tilt", "full_tilt"};

    const char *const SOUND_TILT_DOWN = "tilt_down.big_dripleaf";
    const char *const SOUND_TILT_UP = "tilt_up.big_dripleaf";

    // the leaf's top drops as it tilts; a full tilt lets everything through
    const float LEAF_BOTTOM = 0.6875f;
    const float LEAF_TOP = 0.9375f;
    const float PARTIAL_LEAF_TOP = 0.8125f;

    bool isSoil(const std::string &name) {
        return name == BlockNames::CLAY || name == BlockNames::COARSE_DIRT || name == BlockNames::DIRT
               || name == BlockNames::DIRT_WITH_ROOTS || name == BlockNames::FARMLAND || name == BlockNames::GRASS_BLOCK
               || name == BlockNames::MOSS_BLOCK || name == BlockNames::MUD || name == BlockNames::MUDDY_MANGROVE_ROOTS
               || name == BlockNames::MYCELIUM || name == BlockNames::PODZOL;
    }

    bool isDripleaf(const BlockState *state) {
        return state != nullptr && state->mName == BigDripleafBlock::IDENTIFIER;
    }

    Vector3i offsetY(const Vector3i &position, int32_t dy) {
        return Vector3i(position.x, position.y + dy, position.z);
    }

    // the water the block replaces stays around it, in the second layer
    bool place(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state) {
        const BlockState previous = level.getBlockState(position.x, position.y, position.z);
        if (!BlockChangeSystem::change(level, position, state, BlockChangeCause::Grow, true))
            return false;
        if (LiquidView(previous).isWater()) {
            level.setBlockStateAtLayer(position.x, position.y, position.z, 1, previous);
            BlockActionHandler::broadcastBlockUpdate(owner, level, position, previous, 1);
        }
        return true;
    }

    // the sounds play at the block's corner, as the game does
    Vector3f cornerOf(const Vector3i &position) {
        return Vector3f((float) position.x, (float) position.y, (float) position.z);
    }
}

bool BigDripleafBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

BigDripleafBlock::Tilt BigDripleafBlock::getTilt(const BlockState &state) {
    const std::string tilt = state.mStates.getString(STATE_TILT, TILT_NAMES[0]);
    for (uint8_t index = 0; index < 4; ++index) {
        if (tilt == TILT_NAMES[index])
            return (Tilt) index;
    }
    return Tilt::None;
}

bool BigDripleafBlock::isHead(const BlockState &state) {
    return state.mStates.getByte(STATE_HEAD, 0) != 0;
}

BlockState BigDripleafBlock::withParts(const BlockState &state, bool head, Tilt tilt) {
    Tag states = state.mStates;
    states.putByte(STATE_HEAD, head ? 1 : 0);
    states.putString(STATE_TILT, TILT_NAMES[(uint8_t) tilt]);
    return BlockState(IDENTIFIER, states);
}

bool BigDripleafBlock::canGrowInto(const BlockState &state) {
    return state.mName == BlockNames::AIR || LiquidView(state).isWater()
           || state.mName == BlockNames::SMALL_DRIPLEAF_BLOCK;
}

bool BigDripleafBlock::growFromSmall(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const std::string &direction, const Vector3i *upperHalf) {
    const int32_t limit = RandomTickSystem::nextInt(4) + 2;
    int32_t height = 0;
    while (height < limit && position.y + height <= level.getMaxY()) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y + height, position.z);
        if (state == nullptr || !canGrowInto(*state))
            break;
        ++height;
    }

    Tag states = VanillaBlocks::BIG_DRIPLEAF().toBlockState().mStates;
    states.putString(STATE_DIRECTION, direction);
    const BlockState base(IDENTIFIER, states);
    const BlockState head = withParts(base, true, Tilt::None);
    const BlockState stem = withParts(base, false, Tilt::None);

    // a refused growth leaves the small dripleaf whole
    if (height == 0 || !BlockChangeSystem::allows(level, position, head, BlockChangeCause::Grow))
        return false;
    if (upperHalf != nullptr)
        level.setBlock(*upperHalf, BlockState(BlockNames::AIR), true);

    // each new leaf turns the one under it into a stem
    for (int32_t dy = 0; dy < height; ++dy) {
        const Vector3i at = offsetY(position, dy);
        if (!place(owner, level, at, head))
            return dy > 0;
        const Vector3i below = offsetY(at, -1);
        if (isDripleaf(level.peekBlockPtr(below.x, below.y, below.z)))
            level.setBlock(below, stem, true);
    }
    return true;
}

// the top of the column must be a leaf, and the column must stand on soil
bool BigDripleafBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    const BlockState *top = &state;
    for (int32_t y = position.y + 1; y <= level.getMaxY(); ++y) {
        const BlockState *above = level.peekBlockPtr(position.x, y, position.z);
        if (!isDripleaf(above))
            break;
        top = above;
    }
    if (!isHead(*top))
        return false;

    const BlockState *below = level.peekBlockPtr(position.x, position.y - 1, position.z);
    if (below == nullptr)
        return true;
    return isDripleaf(below) || isSoil(below->mName);
}

bool BigDripleafBlock::watchesNeighbour(const Vector3i &position, const BlockState &state,
                                        const Vector3i *changed) const {
    (void) state;
    return changed == nullptr
           || (changed->x == position.x && changed->z == position.z && std::abs(changed->y - position.y) == 1);
}

// the game checks survival on its next tick, not at once
void BigDripleafBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state) const {
    (void) owner;
    if (!canSurvive(level, position, state))
        level.scheduleUpdate(position, 1);
}

// a leaf placed on a leaf takes its direction and turns it into a stem
void BigDripleafBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    (void) owner;
    (void) state;
    const Vector3i belowPosition = offsetY(position, -1);
    const BlockState *below = level.peekBlockPtr(belowPosition.x, belowPosition.y, belowPosition.z);
    if (!isDripleaf(below))
        return;

    const BlockState from = *below;
    level.setBlock(position, withParts(from, true, Tilt::None), true);
    level.setBlock(belowPosition, withParts(from, false, Tilt::None), true);
}

void BigDripleafBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    if (!canSurvive(level, position, state)) {
        BlockActionHandler::destroyBlock(owner, level, position, state, true, ItemStack::air());
        return;
    }
    if (!isHead(state))
        return;

    if (RedstoneSystem::isGettingPower(owner, level, position)) {
        resetTilt(owner, level, position, state);
        return;
    }

    switch (getTilt(state)) {
        case Tilt::Unstable:
            setTilt(owner, level, position, state, Tilt::Partial);
            break;
        case Tilt::Partial:
            setTilt(owner, level, position, state, Tilt::Full);
            break;
        case Tilt::Full:
            resetTilt(owner, level, position, state);
            break;
        case Tilt::None:
            break;
    }
}

// power holds the leaf flat
void BigDripleafBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    if (!isHead(state) || getTilt(state) == Tilt::None)
        return;
    if (RedstoneSystem::isGettingPower(owner, level, position))
        resetTilt(owner, level, position, state);
}

void BigDripleafBlock::onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                const BlockState &state) const {
    if (!isHead(state) || getTilt(state) != Tilt::None)
        return;
    if (actor.getPosition().y <= (float) position.y + TILT_HEIGHT)
        return;
    setTilt(owner, owner.getLevelFor(actor), position, state, Tilt::Unstable);
}

bool BigDripleafBlock::onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state, ServerActor &projectile) const {
    (void) projectile;
    if (isHead(state) && getTilt(state) != Tilt::Full)
        setTilt(owner, level, position, state, Tilt::Full);
    return false;
}

bool BigDripleafBlock::getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const {
    (void) actor;
    const Tilt tilt = getTilt(state);
    if (!isHead(state) || tilt == Tilt::Full) {
        shape = AxisAlignedBB(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return true;
    }
    shape = AxisAlignedBB(0.0f, LEAF_BOTTOM, 0.0f, 1.0f, tilt == Tilt::Partial ? PARTIAL_LEAF_TOP : LEAF_TOP, 1.0f);
    return true;
}

// the column grows one block: the leaf moves up, a stem takes its place
bool BigDripleafBlock::onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state) const {
    (void) state;
    int32_t top = position.y;
    while (top + 1 <= level.getMaxY() && isDripleaf(level.peekBlockPtr(position.x, top + 1, position.z)))
        ++top;
    if (top + 1 > level.getMaxY())
        return false;

    const Vector3i leaf(position.x, top, position.z);
    const Vector3i above = offsetY(leaf, 1);
    const BlockState *target = level.peekBlockPtr(above.x, above.y, above.z);
    if (target == nullptr || !canGrowInto(*target))
        return false;

    const BlockState head = level.getBlockState(leaf.x, leaf.y, leaf.z);
    if (!place(owner, level, above, withParts(head, true, Tilt::None)))
        return false;
    level.setBlock(leaf, withParts(head, false, Tilt::None), true);
    return true;
}

// unstable and partial come back after 10 ticks, full after 100; only the visible tilts make a sound
void BigDripleafBlock::setTilt(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state, Tilt tilt) {
    // the neighbours hear of it, the leaf itself does not: power only acts on its next tick
    level.setBlock(position, withParts(state, true, tilt), false);
    level.updateAround(position);
    // a projectile can tilt a leaf already counting down: its earlier deadline must not reset it
    level.cancelUpdate(position);
    level.scheduleUpdate(position, tilt == Tilt::Full ? FULL_TICKS
                                   : tilt == Tilt::Partial ? PARTIAL_TICKS
                                                           : UNSTABLE_TICKS);
    if (tilt == Tilt::Unstable)
        return;

    owner.playLevelSound(level, SOUND_TILT_DOWN, cornerOf(position));
    level.postGameEvent(GameEvents::BLOCK_CHANGE, VectorMath::centerOf(position));
}

// an unstable leaf goes back quietly
void BigDripleafBlock::resetTilt(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state) {
    if (getTilt(state) != Tilt::Unstable)
        owner.playLevelSound(level, SOUND_TILT_UP, cornerOf(position));
    level.setBlock(position, withParts(state, true, Tilt::None), false);
    level.updateAround(position);
    level.postGameEvent(GameEvents::BLOCK_CHANGE, VectorMath::centerOf(position));
    level.cancelUpdate(position);
}
