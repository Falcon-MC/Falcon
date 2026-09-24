#pragma once

#include "Block/Block.h"

#include <string>

class EndPortalBlock final : public Block {
public:
    static constexpr int32_t END_PLATFORM_X = 100;

    static constexpr int32_t END_PLATFORM_Y = 50;

    static constexpr int32_t END_PLATFORM_Z = 0;

    explicit EndPortalBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;

    static void spawnObsidianPlatform(Level &level, const Vector3i &position, ServerNetworkHandler *owner);
};
