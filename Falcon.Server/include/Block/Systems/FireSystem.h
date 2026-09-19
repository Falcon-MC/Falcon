#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>

class BlockState;
class Level;
class ServerNetworkHandler;

class FireSystem {
public:
    static constexpr int MAX_AGE = 15;
    static constexpr int TICK_RATE = 30;
    static constexpr int COMBUST_TICKS = 160;
    static constexpr float CONTACT_DAMAGE = 1.0f;
    static constexpr int UNBURNABLE = -1;

    static bool matches(const std::string &identifier);

    static int getBurnChance(const std::string &identifier);

    static int getBurnAbility(const std::string &identifier);

    static bool canBeIgnitedAgainst(const std::string &identifier);

    static bool canSurviveAt(Level &level, const Vector3i &position);

    static bool ignite(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                       bool canLightPortal = false);

    static void onNormalUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state);

    static void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state);

    static void scheduleUpdate(Level &level, const Vector3i &position, int64_t delay);

    static void tick(ServerNetworkHandler &owner, Level &level);
};
