#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// the sixteen colours of concrete powder; falling is FallingBlockSystem's
class ConcretePowderBlock : public Block {
public:
    explicit ConcretePowderBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool usesBlockTicks() const override {
        return true;
    }

    void onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;

    void onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                const Vector3i &changed) const override;

    void onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;

private:
    void _harden(LiquidPhysicsSystem &ticks, const Vector3i &position) const;
};
