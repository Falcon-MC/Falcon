#pragma once

#include "World/Actor/Spawn/SpawnRules.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Level;
class ServerNetworkHandler;

/**
 * Natural mob spawning as the dedicated server does it: every ticking chunk rolls for a spawn
 * column, and each floor of that column spawns one herd against per-pool caps that are counted
 * separately for surface and underground mobs in the surrounding chunks.
 */
class NaturalSpawner {
public:
    static const int32_t POOL_COUNT = 7;

    void tick(ServerNetworkHandler &owner, Level &level);

private:
    struct Counts {
        int32_t mPools[2][POOL_COUNT] = {};
        std::unordered_map<std::string, int32_t> mTypes[2];
    };

    struct SpawnSite {
        Vector3i mPosition;
        bool mSurface = false;
        bool mWater = false;
        bool mInWater = false;
        bool mLava = false;
        bool mInLava = false;
        bool mBubble = false;
        bool mSnow = false;
        std::string mBelow;
        int32_t mBiome = 0;
        int32_t mLight = 0;
        int32_t mLightWithoutWeather = 0;
        float mPlayerDistance = 0.0f;
    };

    void _despawn(ServerNetworkHandler &owner, Level &level, const std::vector<Vector3f> &players);

    void _tickChunk(ServerNetworkHandler &owner, Level &level, int32_t chunkX, int32_t chunkZ, int32_t difficulty,
                    int32_t mobCount, const std::vector<Vector3f> &players);

    void _count(ServerNetworkHandler &owner, Level &level, int32_t chunkX, int32_t chunkZ, Counts &counts);

    void _spawnCluster(ServerNetworkHandler &owner, Level &level, const SpawnSite &site, int32_t difficulty,
                       int32_t mobCount, Counts &counts);

    bool _makeSite(Level &level, const Vector3i &position, const std::vector<Vector3f> &players, bool surface,
                   SpawnSite &site);

    bool _matches(Level &level, const SpawnSite &site, const SpawnCondition &condition, int32_t difficulty) const;

    bool _canSpawnAt(Level &level, const SpawnSite &site, const SpawnCondition &condition) const;

    int32_t _nextInt(int32_t bound);

    float _nextFloat();

    std::unordered_set<int64_t> mSurfaceMobs;
    std::mt19937 mRandom{std::random_device{}()};
};
