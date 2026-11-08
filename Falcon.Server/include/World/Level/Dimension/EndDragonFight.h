#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <optional>
#include <vector>

class Actor;
class EndCrystalActor;
class EnderDragon;
class Level;
class ServerNetworkHandler;
struct EndSpike;

/**
 * The fight of one End: when the dragon spawns, what its death leaves behind and the respawn ritual. The game keeps
 * it on its End dimension and saves it in the dimension's "TheEnd" data (sub_147170120, sub_1471706E0).
 */
class EndDragonFight {
public:
    // the stages of the respawn ritual, saved only as IsRespawning
    enum class RespawnStage : int32_t {
        None = 0,
        Start = 1,
        PreparingToSummonPillars = 2,
        SummoningPillars = 3,
        SummoningDragon = 4
    };

    // one ritual tick from its stage and ticks spent in it (sub_14716C8B0)
    struct RespawnStep {
        RespawnStage mNextStage = RespawnStage::None;
        bool mRestartTimer = false;
        bool mBeamToSummonPoint = false;
        bool mPlaySummonSound = false;
        int32_t mBeamToSpike = -1;
        int32_t mClearSpike = -1;
        int32_t mRebuildSpike = -1;
        bool mSummonDragon = false;
    };

    static constexpr const char *SAVED_DATA_KEY = "TheEnd";

    static constexpr int32_t GATEWAY_COUNT = 20;

    static constexpr int32_t GATEWAY_RADIUS = 96;

    static constexpr int32_t GATEWAY_Y = 75;

    static constexpr int32_t FIRST_KILL_EXPERIENCE = 12000;

    static constexpr int32_t REPEAT_KILL_EXPERIENCE = 500;

    static constexpr int32_t DEATH_DURATION = 200;

    explicit EndDragonFight(int64_t seed);

    static RespawnStep stepRespawn(RespawnStage stage, int32_t ticks, size_t spikeCount);

    // opening order: 0 to 19 shuffled with the level seed (sub_147170120)
    static std::vector<int32_t> createGatewayOrder(int64_t seed);

    static Vector3i getGatewayPosition(int32_t index);

    // per tick of the death (sub_144AE8C40)
    static int32_t getDeathExperience(int32_t deathTicks, bool previouslyKilled);

    // dragon spawn point, aimed at by the respawn crystals (fight constructor, sub_14716B720)
    static Vector3i getSummonPoint();

    void load(const Tag &dimensionData);

    void save(Tag &dimensionData) const;

    void tick(ServerNetworkHandler &owner, Level &level);

    // on crystal placement: four around the exit portal start the respawn
    void tryRespawn(ServerNetworkHandler &owner, Level &level);

    void onCrystalDestroyed(ServerNetworkHandler &owner, Level &level, EndCrystalActor &crystal, Actor *attacker);

    /** The end of the dragon's death: exit portal, next gateway and, the first time, the egg (sub_14717D450). */
    void onDragonKilled(ServerNetworkHandler &owner, Level &level, const EnderDragon &dragon);

    bool isDragonKilled() const {
        return mDragonKilled;
    }

    bool wasPreviouslyKilled() const {
        return mPreviouslyKilled;
    }

    bool isDragonSpawned() const {
        return mDragonSpawned;
    }

    int64_t getDragonUniqueId() const {
        return mDragonUniqueId;
    }

    RespawnStage getRespawnStage() const {
        return mRespawnStage;
    }

    const std::optional<Vector3i> &getExitPortalLocation() const {
        return mExitPortalLocation;
    }

    const std::vector<int32_t> &getGateways() const {
        return mGateways;
    }

private:
    void _initializeDragon(ServerNetworkHandler &owner, Level &level);

    void _attachDragon(EnderDragon &dragon);

    EnderDragon *_findDragon(ServerNetworkHandler &owner, Level &level);

    void _tickDragon(ServerNetworkHandler &owner, Level &level);

    void _tickRespawn(ServerNetworkHandler &owner, Level &level);

    void _spawnExitPortal(ServerNetworkHandler &owner, Level &level, bool active);

    bool _findExitPortalLocation(Level &level);

    void _spawnNewGateway(ServerNetworkHandler &owner, Level &level);

    void _placePendingGateways(Level &level);

    void _updateCrystalCount(ServerNetworkHandler &owner, Level &level);

    void _resetSpikeCrystals(ServerNetworkHandler &owner, Level &level);

    void _setRespawnBeams(ServerNetworkHandler &owner, const std::optional<Vector3i> &target);

    void _rebuildSpike(ServerNetworkHandler &owner, Level &level, const EndSpike &spike);

    void _clearSpikeArea(ServerNetworkHandler &owner, Level &level, const EndSpike &spike);

    void _finishRespawn(ServerNetworkHandler &owner, Level &level);

    bool _isArenaLoaded(Level &level) const;

    std::vector<EndCrystalActor *> _findCrystals(ServerNetworkHandler &owner, Level &level, float minX, float minY,
                                                 float minZ, float maxX, float maxY, float maxZ);

    std::vector<int32_t> &_gatewayOrder();

    int64_t mSeed;
    bool mDragonKilled = false;
    bool mPreviouslyKilled = false;
    bool mDragonSpawned = false;
    uint8_t mVersion = 0;
    int64_t mDragonUniqueId = -1;
    std::optional<Vector3i> mExitPortalLocation;
    std::vector<int32_t> mGateways;
    std::vector<int32_t> mPendingGateways;
    RespawnStage mRespawnStage = RespawnStage::None;
    int32_t mRespawnTicks = 0;
    std::vector<int64_t> mRespawnCrystals;
    int32_t mCrystalsAlive = 0;
    int32_t mDragonCheckTicks = 0;
    int32_t mPortalCheckTicks = 0;
};
