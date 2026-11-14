#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Raid.h"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

class Actor;
class Level;
class MobActor;
class ServerNetworkHandler;
class ServerPlayer;

// order of the dweller_role names of minecraft:dweller
enum class DwellerRole : int32_t { Inhabitant = 0, Defender = 1, Hostile = 2, Passive = 3, None = 4 };

// created and removed by its level's VillageManager
class Village : public std::enable_shared_from_this<Village> {
public:
    static const size_t POI_TYPE_COUNT = 3;
    static const size_t ROLE_COUNT = 4;

    Village();

    const AxisAlignedBB &getBounds() const {
        return mBounds;
    }

    Vector3f getCenter() const;

    // box grown by margin on every side, edges included
    bool contains(float margin, const Vector3f &position) const;

    // false when full or its type already holds 32; weak refs only: the VillageManager owns every poi
    bool addPoi(const std::shared_ptr<PoiInstance> &poi);

    void addDweller(ServerNetworkHandler &owner, DwellerRole role, int64_t id);

    void removeDweller(DwellerRole role, int64_t id);

    void markDwellerSeen(DwellerRole role, int64_t id);

    bool hasDweller(DwellerRole role, int64_t id) const;

    std::vector<int64_t> getDwellerIds(DwellerRole role) const;

    size_t getInhabitantCount() const {
        return mRoles[(size_t) DwellerRole::Inhabitant].size();
    }

    bool isInitialized() const {
        return mInitialized;
    }

    int32_t getTickCount() const {
        return mTickCount;
    }

    bool hasUnclaimedBed() const {
        return !mUnclaimed[(size_t) PoiType::Bed].empty();
    }

    // unclaimed beds plus beds claimed by inhabitants; like the game, a queued bed counts even
    // broken: only claimed pois are validated
    size_t countBeds() const;

    std::vector<std::shared_ptr<PoiInstance>> getPois() const;

    void tick(ServerNetworkHandler &owner, Level &level, int64_t currentTick);

    Raid *getRaid() {
        return mRaid.get();
    }

    const Raid *getRaid() const {
        return mRaid.get();
    }

    // won or lost, only waiting for the celebration to end
    bool isRaidFinished() const {
        return mRaid != nullptr && mRaid->getResult() != Raid::Result::None;
    }

    // edges included: the village box grown by 32 when the raid started; players inside follow the raid
    bool isInRaidBounds(const Vector3f &position) const;

    // remembered for 300 ticks after its last hit on an inhabitant or a defender
    void addAggressor(uint64_t runtimeId, int64_t tick);

    // clamped to -30..10
    void changeStanding(int64_t playerId, int32_t delta);

    int32_t getStanding(int64_t playerId) const;

    // an inhabitant at its job site; only inhabitants are recorded
    void recordWork(int64_t id, int64_t tick);

    // inhabitants that worked within the last day
    size_t countRecentWorkers(int64_t currentTick) const;

    // the game's condition: 20 beds and at least one per inhabitant, three quarters of them at work within a day,
    // fewer defenders than a tenth of the inhabitants
    bool wantsGolem(int64_t currentTick) const;

    // the closest remembered aggressor or hostile dweller, any distance
    Actor *findClosestAggressor(ServerNetworkHandler &owner, const Actor &from) const;

    // the closest player whose standing fell to -15 or lower, any distance
    ServerPlayer *findClosestHostilePlayer(ServerNetworkHandler &owner, const Actor &from) const;

    // no-op while a raid runs or on peaceful
    void tryStartRaid(ServerNetworkHandler &owner);

    // expires after a 48001-tick wave, dropped on peaceful, else advanced
    void tickRaid(ServerNetworkHandler &owner, Level &level, int64_t currentTick);

private:
    using PoiQueue = std::vector<std::weak_ptr<PoiInstance>>;

    struct Inhabitant {
        int64_t mId;
        std::array<std::weak_ptr<PoiInstance>, POI_TYPE_COUNT> mPois;
    };

    struct DwellerEntry {
        int64_t mId;
        int64_t mLastSeenTick;
        Vector3i mPosition;
    };

    Inhabitant *_findInhabitant(int64_t id);

    Inhabitant &_findOrAddInhabitant(int64_t id);

    void _releaseInhabitant(int64_t id);

    void _claim(ServerNetworkHandler &owner, int64_t id, const std::shared_ptr<PoiInstance> &poi);

    void _claimNext(ServerNetworkHandler &owner, int64_t id, PoiQueue &queue);

    void _claimJobSite(ServerNetworkHandler &owner, int64_t id);

    void _spawnPoiParticles(ServerNetworkHandler &owner, MobActor &mob, int32_t data) const;

    void _tickPoiValidity(ServerNetworkHandler &owner, Level &level);

    void _tickDwellerTimeouts(ServerNetworkHandler &owner, Level &level);

    void _recalculateBounds();

    void _tickAggressors(ServerNetworkHandler &owner);

    void _trySpawnGolem(ServerNetworkHandler &owner, Level &level);

    void _decayStandings(int64_t currentTick);

    std::array<PoiQueue, POI_TYPE_COUNT> mUnclaimed;
    std::vector<Inhabitant> mInhabitants;
    std::array<std::vector<DwellerEntry>, ROLE_COUNT> mRoles;
    AxisAlignedBB mBounds;
    AxisAlignedBB mRaidBounds;
    std::unique_ptr<Raid> mRaid;
    // runtime id of the aggressor, tick of its last hit
    std::unordered_map<uint64_t, int64_t> mAggressors;
    // by player unique id
    std::unordered_map<int64_t, int32_t> mStandings;
    // inhabitant unique id, tick it last worked
    std::vector<std::pair<int64_t, int64_t>> mWorkers;
    int64_t mNextNegativeDecayTick = 0;
    int64_t mNextPositiveDecayTick = 0;
    int64_t mLastTick = 0;
    int32_t mTickCount = 0;
    bool mInitialized = false;
};
