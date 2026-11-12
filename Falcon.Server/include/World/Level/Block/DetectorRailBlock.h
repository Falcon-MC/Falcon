#pragma once

#include "World/Level/Block/BaseRailBlock.h"

#include <string>

class DetectorRailBlock final : public BaseRailBlock {
public:
    static constexpr int CHECK_DELAY = 20;

    explicit DetectorRailBlock(const Block &base) : BaseRailBlock(base) {}

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override {
        return true;
    }

    int getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                         bool strong) const override;

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    static void checkPressed(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state);
};
