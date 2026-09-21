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

class EndPortalFrameBlock final : public Block {
public:
    explicit EndPortalFrameBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool tryCompletePortal(Level &level, const Vector3i &framePosition, ServerNetworkHandler *owner);
};

class ObsidianBlock final : public Block {
public:
    explicit ObsidianBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    static bool tryLightPortal(Level &level, const Vector3i &firePosition, ServerNetworkHandler *owner);

private:
    static bool lightPortalAtBase(Level &level, const Vector3i &position, ServerNetworkHandler *owner);
};
