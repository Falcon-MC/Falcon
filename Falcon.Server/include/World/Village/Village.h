#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Raid.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class Level;
class MobActor;
class ServerNetworkHandler;

// order of the dweller_role names of minecraft:dweller
enum class DwellerRole : int32_t {
    Inhabitant = 0,
    Defender = 1,
    Hostile = 2,
    Passive = 3,
    None = 4
};

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

    // unclaimed beds plus beds claimed by inhabitants; like the game (sub_1439E5420), a queued bed counts even
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

    std::array<PoiQueue, POI_TYPE_COUNT> mUnclaimed;
    std::vector<Inhabitant> mInhabitants;
    std::array<std::vector<DwellerEntry>, ROLE_COUNT> mRoles;
    AxisAlignedBB mBounds;
    AxisAlignedBB mRaidBounds;
    std::unique_ptr<Raid> mRaid;
    int64_t mLastTick = 0;
    int32_t mTickCount = 0;
    bool mInitialized = false;
};
