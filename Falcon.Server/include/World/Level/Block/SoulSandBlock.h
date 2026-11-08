#pragma once

#include "World/Level/Block/Block.h"

class SoulSandBlock : public Block {
public:
    explicit SoulSandBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    float getSpeedFactor() const override {
        return 0.4f;
    }

    float getAccelerationFrictionMultiplier() const override {
        return 1.225f;
    }

    bool usesBlockTicks() const override {
        return true;
    }

    void onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;

    void onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                const Vector3i &changed) const override;

    void onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;
};
