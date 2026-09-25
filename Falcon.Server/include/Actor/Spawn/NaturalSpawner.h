#pragma once

#include "Actor/Spawn/SpawnRules.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

class Level;
class ServerNetworkHandler;

class NaturalSpawner {
public:
    struct Category {
        const char *mName;
        int32_t mCap;
        int32_t mInterval;
        bool mHostile;
    };

    void tick(ServerNetworkHandler &owner, Level &level);

private:
    struct NearbyActor {
        Vector3f mPosition;
        const std::string *mPopulation;
        std::string mIdentifier;
    };

    struct SpawnSite {
        Vector3i mPosition;
        bool mSurface = false;
        bool mWater = false;
        bool mLava = false;
        bool mBubble = false;
        bool mSnow = false;
        std::string mBelow;
        int32_t mBiome = 0;
        int32_t mLight = 0;
        int32_t mLightWithoutWeather = 0;
        float mPlayerDistance = 0.0f;
    };

    void _despawn(ServerNetworkHandler &owner, Level &level, const std::vector<Vector3f> &players);

    void _attempt(ServerNetworkHandler &owner, Level &level, const Vector3f &player, const Category &category,
                  int32_t difficulty, const std::vector<Vector3f> &players, const std::vector<NearbyActor> &nearby);

    bool _makeSite(Level &level, const Vector3i &position, const std::vector<Vector3f> &players, bool water,
                   SpawnSite &site);

    bool _matches(Level &level, const SpawnSite &site, const SpawnCondition &condition, int32_t difficulty,
                  const std::string &identifier, const Vector3f &player, const std::vector<NearbyActor> &nearby) const;

    int32_t _nextInt(int32_t bound);

    std::mt19937 mRandom{std::random_device{}()};
};
