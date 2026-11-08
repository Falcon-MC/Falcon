#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

class Actor;
class EndCrystalActor;
class EndGatewayBlockActor;
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

    /**
     * Gives the gateway at position an exit and a gateway back from it once both ends are loaded, placing the gateway
     * itself too when placeGateway (sub_14714F240). The game aims the exit right away from the terrain generator.
     */
    void requestGatewayExit(const Level &level, const Vector3i &gateway, bool placeGateway);

    /**
     * Loads both ends of a gateway that already has an exit and moves an exit that would leave an entity in a block
     * or over the void, at either end (sub_14716A610, sub_14716AEC0).
     */
    void requestGatewayCheck(const Vector3i &gateway, const Vector3i &exit);

    /** The rough exit of a gateway, from the End's terrain generator alone (sub_14714F400). */
    static Vector3i findRoughExit(const Level &level, const Vector3i &gateway);

    size_t getGatewayRequestCount() const {
        return mGatewayRequests.size();
    }

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
    struct GatewayRequest {
        bool mCheckOnly;
        Vector3i mGateway;
        Vector3i mExit;
        bool mPlaceGateway;
    };

    void _tickGatewayRequests(ServerNetworkHandler &owner, Level &level);

    bool _loadGatewayEnds(Level &level, const GatewayRequest &request, bool registerLoaders);

    std::vector<std::pair<int32_t, int32_t>> _loadGatewayBlockActors(ServerNetworkHandler &owner, Level &level,
                                                                     const GatewayRequest &request);

    void _releaseGatewayEnds(ServerNetworkHandler &owner, Level &level, const GatewayRequest &request,
                             const std::vector<std::pair<int32_t, int32_t>> &blockActorChunks);

    void _findGatewayExit(ServerNetworkHandler &owner, Level &level, const GatewayRequest &request);

    void _checkGatewayExit(ServerNetworkHandler &owner, Level &level, const Vector3i &from, const Vector3i &to,
                           bool fromNearestGateway);

    EndGatewayBlockActor &_placeGateway(Level &level, const Vector3i &position);

    void _raiseIsland(Level &level, const Vector3i &position);

    void _initializeDragon(ServerNetworkHandler &owner, Level &level);

    void _attachDragon(EnderDragon &dragon);

    EnderDragon *_findDragon(ServerNetworkHandler &owner, Level &level);

    void _tickDragon(ServerNetworkHandler &owner, Level &level);

    void _tickRespawn(ServerNetworkHandler &owner, Level &level);

    void _spawnExitPortal(ServerNetworkHandler &owner, Level &level, bool active);

    bool _findExitPortalLocation(Level &level);

    void _spawnNewGateway(Level &level);

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
    // the front request is the one whose ends are loading
    std::deque<GatewayRequest> mGatewayRequests;
    bool mGatewayEndsRequested = false;
    RespawnStage mRespawnStage = RespawnStage::None;
    int32_t mRespawnTicks = 0;
    std::vector<int64_t> mRespawnCrystals;
    int32_t mCrystalsAlive = 0;
    int32_t mDragonCheckTicks = 0;
    int32_t mPortalCheckTicks = 0;
};
