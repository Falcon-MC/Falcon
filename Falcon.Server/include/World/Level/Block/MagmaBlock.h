#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class MagmaBlock final : public Block {
public:
    explicit MagmaBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    PathHazard getPathHazard() const override {
        return PathHazard::Damaging;
    }

    void onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                  const BlockState &state) const override;

    bool usesBlockTicks() const override {
        return true;
    }

    void onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                const Vector3i &changed) const override;

    void onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;
};
