#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Level/Dimension.h"

#include <cstdint>
#include <string>

class Level;
class ServerNetworkHandler;
class ServerPlayer;

class PortalForcer {
public:
    static constexpr int32_t PORTAL_DELAY_TICKS = 80;

    static constexpr int32_t PORTAL_COOLDOWN_TICKS = 300;

    static constexpr int32_t PORTAL_SEARCH_RADIUS = 128;

    static constexpr int32_t MAX_PORTAL_SIZE = 23;

    static constexpr int32_t NETHER_ROOF_LIMIT = 115;

    static constexpr int32_t END_PLATFORM_X = 100;

    static constexpr int32_t END_PLATFORM_Y = 50;

    static constexpr int32_t END_PLATFORM_Z = 0;

    static constexpr int32_t MAX_PORTAL_BLOCKS = 4096;

    static void tickPlayer(ServerNetworkHandler &owner, ServerPlayer &player);

    static bool tryLightPortal(Level &level, const Vector3i &position, ServerNetworkHandler *owner = nullptr);

    /**
     * Closes the portal a broken block belonged to: every nether portal block touching it, and, when the broken
     * block was an end portal or one of its frames, every end portal block touching it.
     */
    static void onFrameBlockBroken(Level &level, const Vector3i &position, const std::string &brokenIdentifier,
                                   ServerNetworkHandler *owner = nullptr);

    static bool tryInsertEnderEye(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position);

    static bool tryCompleteEndPortal(Level &level, const Vector3i &framePosition,
                                     ServerNetworkHandler *owner = nullptr);

    static void spawnPortal(Level &level, const Vector3i &position, ServerNetworkHandler *owner = nullptr);

    static void spawnObsidianPlatform(Level &level, const Vector3i &position, ServerNetworkHandler *owner = nullptr);

    static bool findNearestPortal(Level &level, const Vector3i &origin, Vector3i &out);

    static bool findNetherPortalDestination(Level &destination, const Vector3i &source, Vector3i &out);

    static bool isNetherPortalBlock(Level &level, const Vector3i &position);

    static bool isEndPortalBlock(Level &level, const Vector3i &position);
};
