#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <random>
#include <vector>

namespace json {
    class Value;
}

class Level;
class ServerActor;
class ServerNetworkHandler;

// the burst of a wind charge: minecraft:explode with no damage and no broken block, pushing what it reaches
// and working the doors, levers and bells its rays meet
class WindBurst {
public:
    struct Settings {
        float mPower = 1.2f;
        float mKnockbackScaling = 1.0f;
        float mMaxResistance = 0.0f;
        bool mTogglesBlocks = false;
        bool mNegatesFallDamage = false;
        bool mAllowUnderwater = false;
        // a mob's charge works blocks only while mobGriefing is on
        bool mNeedsMobGriefing = false;
    };

    // the minecraft:explode component as the definition writes it
    static Settings fromComponent(const json::Value &component);

    static void burst(ServerNetworkHandler &owner, Level &level, const Vector3f &center, const Settings &settings,
                      const ServerActor *source);

    // the blocks its rays reach, each once, in the order first reached
    static std::vector<Vector3i> reachedBlocks(Level &level, const Vector3f &center, const Settings &settings,
                                               std::mt19937 &random);

    // the push an actor at that distance takes before its knockback resistance, 0 out of reach
    static float impact(float distance, float exposure, const Settings &settings);
};
