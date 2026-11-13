#include "World/Level/Block/SculkSensorBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/SculkSensorBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <array>
#include <cmath>

FALCON_REGISTER_BLOCK(SculkSensorBlock, 352);
FALCON_REGISTER_BLOCK(CalibratedSculkSensorBlock, 353);

namespace {
    const char *STATE_PHASE = "sculk_sensor_phase";
    const char *SOUND_POWER_ON = "power.on.sculk_sensor";
    const char *SOUND_POWER_OFF = "power.off.sculk_sensor";
    const char *SOUND_RESONATE = "resonate.amethyst_block";

    // down, up, north, south, west, east, as the activation walks them
    const std::array<Vector3i, 6> RESONATING_NEIGHBOURS = {Vector3i(0, -1, 0), Vector3i(0, 1, 0),  Vector3i(0, 0, -1),
                                                           Vector3i(0, 0, 1),  Vector3i(-1, 0, 0), Vector3i(1, 0, 0)};
    // semitones by frequency (0x14F3D08B8, 26.60)
    const std::array<int32_t, 16> RESONATE_NOTES = {0, 0, 2, 4, 6, 7, 9, 10, 12, 14, 15, 18, 19, 21, 22, 24};

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    BlockState withPhase(const BlockState &state, int32_t phase) {
        Tag states = state.mStates;
        states.putInt(STATE_PHASE, phase);
        return BlockState(state.mName, states);
    }

    // the clicks stay silent under water (material 5 of the extra block)
    bool isWaterlogged(Level &level, const Vector3i &position) {
        const BlockState extra = level.getBlockStateAtLayer(position.x, position.y, position.z, 1);
        BlockMaterialType material;
        return BlockMaterial::find(extra, material) && material == BlockMaterialType::Water;
    }

    void setPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int power) {
        SculkSensorBlockActor *actor = level.getBlockActors().find<SculkSensorBlockActor>(position);
        if (actor != nullptr)
            actor->setPower(power);
        RedstoneSystem::updateAroundRedstone(owner, level, position);
        RedstoneSystem::updateAroundRedstone(owner, level, RedstoneFace::relative(position, RedstoneFace::DOWN));
    }
}

bool SculkSensorBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SCULK_SENSOR;
}

bool CalibratedSculkSensorBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::CALIBRATED_SCULK_SENSOR;
}

int32_t SculkSensorBlock::phaseOf(const BlockState &state) {
    return state.mStates.getInt(STATE_PHASE, PHASE_INACTIVE);
}

uint8_t SculkSensorBlock::getLiquidBlockedFaces(const BlockState &state) const {
    (void) state;
    return (uint8_t) (1 << LiquidFace::DOWN);
}

const char *SculkSensorBlock::getBlockActorId() const {
    return SculkSensorBlockActor::BLOCK_ACTOR_ID;
}

const char *CalibratedSculkSensorBlock::getBlockActorId() const {
    return CalibratedSculkSensorBlockActor::BLOCK_ACTOR_ID;
}

// the listener lives in the block actor: a sensor placed without one would stay deaf
void SculkSensorBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    (void) owner;
    (void) state;
    level.getBlockActors().getOrCreate<SculkSensorBlockActor>(position);
}

void CalibratedSculkSensorBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                                 const BlockState &state) const {
    (void) owner;
    (void) state;
    level.getBlockActors().getOrCreate<CalibratedSculkSensorBlockActor>(position);
}

void SculkSensorBlock::activate(ServerNetworkHandler &owner, Level &level, const Vector3i &position, Actor *source,
                                int32_t power, int32_t frequency) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    const SculkSensorBlock *block = dynamic_cast<const SculkSensorBlock *>(VanillaBlocks::fromState(state));
    if (block == nullptr)
        return;

    level.setBlock(position, withPhase(state, PHASE_ACTIVE), true);
    setPower(owner, level, position, power);
    if (!isWaterlogged(level, position))
        owner.playLevelSound(level, SOUND_POWER_ON, centerOf(position));
    level.cancelUpdate(position);

    for (const Vector3i &offset: RESONATING_NEIGHBOURS) {
        const Vector3i neighbour(position.x + offset.x, position.y + offset.y, position.z + offset.z);
        const BlockState *next = level.peekBlockPtr(neighbour.x, neighbour.y, neighbour.z);
        if (next == nullptr || next->mName != BlockNames::AMETHYST_BLOCK)
            continue;

        level.postGameEvent(GameEvents::resonate(frequency), centerOf(neighbour), source);
        int32_t pitch = 0;
        if (frequency >= 0 && frequency <= 15)
            pitch = (int32_t) (std::exp2(((float) RESONATE_NOTES[(size_t) frequency] - 12.0f) / 12.0f) * 1000.0f);
        owner.playLevelSound(level, SOUND_RESONATE,
                             Vector3f((float) neighbour.x, (float) neighbour.y, (float) neighbour.z), ":", pitch);
    }

    level.postGameEvent(GameEvents::SCULK_SENSOR_TENDRILS_CLICKING, centerOf(position), source);
    level.scheduleUpdate(position, block->getActiveTicks());
}

// SculkSensorBlock tick (sub_14B255200, 26.60): active turns to cooldown and its power drops, cooldown ends
void SculkSensorBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    const int32_t phase = phaseOf(state);
    if (phase == PHASE_ACTIVE) {
        level.setBlock(position, withPhase(state, PHASE_COOLDOWN), true);
        if (!isWaterlogged(level, position))
            owner.playLevelSound(level, SOUND_POWER_OFF, centerOf(position));
        setPower(owner, level, position, 0);
        level.scheduleUpdate(position, COOLDOWN_TICKS);
        return;
    }

    if (phase == PHASE_COOLDOWN)
        level.setBlock(position, withPhase(state, PHASE_INACTIVE), true);
}

// the producer is set up facing down (sub_14B255CE0): every neighbour hears it, only the block below is charged
int SculkSensorBlock::getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                                       bool strong) const {
    if (phaseOf(state) != PHASE_ACTIVE)
        return 0;

    SculkSensorBlockActor *actor = level.getBlockActors().find<SculkSensorBlockActor>(position);
    if (actor == nullptr)
        return 0;
    return !strong || face == RedstoneFace::DOWN ? actor->getPower() : 0;
}

// sub_14B256540: the frequency of the last vibration, while active only
int SculkSensorBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    if (phaseOf(state) != PHASE_ACTIVE)
        return 0;

    SculkSensorBlockActor *actor = level.getBlockActors().find<SculkSensorBlockActor>(position);
    return actor == nullptr ? 0 : actor->listener().getLastEvent().getFrequency();
}
