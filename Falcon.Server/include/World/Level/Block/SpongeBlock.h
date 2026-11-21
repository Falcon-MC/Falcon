#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// sponge and wet_sponge
class SpongeBlock : public Block {
public:
    explicit SpongeBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool usesBlockTicks() const override {
        return true;
    }

    void onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;

    void onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                const Vector3i &changed) const override;

    void onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;

private:
    static bool _isWet(const BlockState &state);
    void _tryAbsorb(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const;
    static bool _absorb(LiquidPhysicsSystem &ticks, const Vector3i &origin);
};
