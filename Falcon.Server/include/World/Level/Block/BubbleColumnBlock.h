#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// bubble_column on layer 0 over its water on layer 1
class BubbleColumnBlock : public Block {
public:
    explicit BubbleColumnBlock(const Block &block) : Block(block) {}

    static constexpr const char *IDENTIFIER = "minecraft:bubble_column";

    static bool matches(const std::string &identifier);

    // the column one higher over a water source; false when nothing grew (sub_1435B5770)
    static bool growAbove(LiquidPhysicsSystem &ticks, const Vector3i &position);

    bool usesBlockTicks() const override {
        return true;
    }

    void onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;

    void onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state,
                                const Vector3i &changed) const override;

    void onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const override;

private:
    static bool _isSupported(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state);
};
