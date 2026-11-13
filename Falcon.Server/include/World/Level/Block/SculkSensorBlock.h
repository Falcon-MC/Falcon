#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <string>

class Actor;

// SculkSensorBlock (vtable 0x14F41B030, 26.60): both sensors are this class, the calibrated one stays active less
class SculkSensorBlock : public Block {
public:
    // sculk_sensor_phase
    static constexpr int32_t PHASE_INACTIVE = 0;
    static constexpr int32_t PHASE_ACTIVE = 1;
    static constexpr int32_t PHASE_COOLDOWN = 2;
    // BlockLegacy +0x378 set at registration (sub_14CE9AD30): 30 for the sensor, 10 for the calibrated one
    static constexpr int32_t ACTIVE_TICKS = 30;
    // sub_14B255200 schedules the cooldown's end 10 ticks on
    static constexpr int32_t COOLDOWN_TICKS = 10;

    explicit SculkSensorBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    static int32_t phaseOf(const BlockState &state);

    // SculkSensorBlock::activate (sub_14B256960, 26.60)
    static void activate(ServerNetworkHandler &owner, Level &level, const Vector3i &position, Actor *source,
                         int32_t power, int32_t frequency);

    uint8_t getLiquidBlockedFaces(const BlockState &state) const override;

    const char *getBlockActorId() const override;

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    bool isSignalSource() const override {
        return true;
    }

    int getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                         bool strong) const override;

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    virtual int32_t getActiveTicks() const {
        return ACTIVE_TICKS;
    }
};

class CalibratedSculkSensorBlock final : public SculkSensorBlock {
public:
    static constexpr int32_t ACTIVE_TICKS = 10;

    explicit CalibratedSculkSensorBlock(const Block &block) : SculkSensorBlock(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    int32_t getActiveTicks() const override {
        return ACTIVE_TICKS;
    }
};
