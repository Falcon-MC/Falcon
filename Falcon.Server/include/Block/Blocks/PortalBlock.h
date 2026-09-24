#pragma once

#include "Block/Block.h"

#include <string>

class PortalBlock final : public Block {
public:
    static constexpr int32_t PORTAL_DELAY_TICKS = 80;

    static constexpr int32_t PORTAL_COOLDOWN_TICKS = 300;

    static constexpr int32_t PORTAL_SEARCH_RADIUS = 128;

    static constexpr int32_t MAX_PORTAL_SIZE = 23;

    static constexpr int32_t NETHER_ROOF_LIMIT = 115;

    explicit PortalBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;

    static void spawnPortal(Level &level, const Vector3i &position, ServerNetworkHandler *owner);

    static bool findNearestPortal(Level &level, const Vector3i &origin, Vector3i &out);

    static bool findDestination(Level &destination, const Vector3i &source, Vector3i &out);
};
