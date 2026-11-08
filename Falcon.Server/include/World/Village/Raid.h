#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <unordered_set>

class Level;
class ServerNetworkHandler;
class Village;

// waves of minecraft:raid_group_<wave> spawn groups until all are cleared (victory), the village loses its beds or
// inhabitants (defeat), or spawning keeps failing; owned and ticked by its village on the main thread
class Raid {
public:
    enum class State : int32_t {
        Cooldown = 0,
        FindingSpawn = 1,
        SpawningWave = 2,
        Ongoing = 3,
        Victory = 4,
        Ended = 5,
        Removed = 6
    };

    enum class Result : int32_t {
        None = 0,
        Victory = 1,
        Defeat = 2
    };

    explicit Raid(uint8_t waveCount);

    void tick(ServerNetworkHandler &owner, Level &level, Village &village, int64_t currentTick);

    State getState() const {
        return mState;
    }

    Result getResult() const {
        return mResult;
    }

    int64_t getTicksInState() const {
        return mTicksInState;
    }

    const std::unordered_set<int64_t> &getRaiders() const {
        return mRaiders;
    }

    bool hasRaider(int64_t id) const {
        return mRaiders.count(id) != 0;
    }

    // sub_14CE8F940: the wave grows by the recruit's health truncated to int
    void addRecruit(int64_t id, float health);

    // killed one of the village's hostile dwellers; rewarded on victory
    void addHero(int64_t playerId) {
        mHeroes.insert(playerId);
    }

    // raiders lose their raid persistence
    void expireRaiders(ServerNetworkHandler &owner) const;

    // 0 to 1
    float getProgress(ServerNetworkHandler &owner) const;

private:
    void _resetStateTimers();

    float _sumRaiderHealth(ServerNetworkHandler &owner) const;

    bool _findSpawn(ServerNetworkHandler &owner, Level &level, const Village &village, int64_t elapsed,
                    Vector3f &position) const;

    bool _spawnWave(ServerNetworkHandler &owner, Level &level, Village &village, int64_t elapsed);

    void _onWaveSpawned(ServerNetworkHandler &owner, Level &level) const;

    void _onVictory(ServerNetworkHandler &owner, Level &level, const Village &village) const;

    void _onDefeat(ServerNetworkHandler &owner) const;

    void _rewardHero(ServerNetworkHandler &owner, int64_t playerId) const;

    void _soundHornNearRaider(ServerNetworkHandler &owner, Level &level) const;

    State mState = State::Cooldown;
    Result mResult = Result::None;
    uint8_t mCurrentWave = 0;
    uint8_t mWaveCount = 0;
    int64_t mLastTick = -1;
    int64_t mTicksInState = 0;
    // timers {300, 40, 40, 600} from xmmword_14F1B3880, then 600 and 600 (0x1439d7ce8), in _createRaid sub_1439D7BF0
    int32_t mCooldownTicks = 300;
    int32_t mNextWaveDelay = 40;
    int32_t mNextWaveCountdown = 40;
    int32_t mHornInterval = 600;
    int32_t mHornCountdown = 600;
    int32_t mCelebrationTicks = 600;
    Vector3f mSpawnPosition;
    std::unordered_set<int64_t> mRaiders;
    float mTotalHealth = 0.0f;
    uint8_t mMaxSpawnFailures = 5; // 0x1439d7d43
    uint8_t mSpawnFailures = 0;
    std::unordered_set<int64_t> mHeroes;
};
