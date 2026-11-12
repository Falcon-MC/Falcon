#pragma once

#include "World/Level/Block/Block.h"

#include <optional>
#include <string>

class LightningRodBlock final : public Block {
public:
    static constexpr int POWERED_TICKS = 8;
    static constexpr int HIT_EVENT = 2033;
    static constexpr int ATTRACT_RADIUS = 64;

    explicit LightningRodBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override {
        return true;
    }

    int getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                         bool strong) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    static void onLightningHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static std::optional<Vector3i> findAttractingRod(Level &level, const Vector3i &target);
};
