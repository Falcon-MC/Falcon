#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// SculkShriekerBlock: shrieks at a sensor's click or a step, warns the players near it and, with enough warnings,
// calls a warden up
class SculkShriekerBlock final : public Block {
public:
    static constexpr const char *STATE_ACTIVE = "active";
    static constexpr const char *STATE_CAN_SUMMON = "can_summon";

    explicit SculkShriekerBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    void onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                  const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;
};
