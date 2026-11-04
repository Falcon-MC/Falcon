#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "World/Village/PoiInstance.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class MobActor;
class ServerNetworkHandler;

/** What a dweller is to its village, in the order of the `dweller_role` names of `minecraft:dweller`. */
enum class DwellerRole : int32_t {
    Inhabitant = 0,
    Defender = 1,
    Hostile = 2,
    Passive = 3,
    None = 4
};

/**
 * A village: the points of interest it owns, the dwellers registered with it and the box they span. Villages are
 * created and removed by the VillageManager of their level.
 */
class Village {
public:
    static const size_t POI_TYPE_COUNT = 3;
    static const size_t ROLE_COUNT = 4;

    Village();

    const AxisAlignedBB &getBounds() const { return mBounds; }

    Vector3f getCenter() const;

    /** Whether position lies in the box grown by margin on every side, edges included. */
    bool contains(float margin, const Vector3f &position) const;

    /** Queues an unclaimed point of interest. Returns false when it is full or its type already holds 32. */
    bool addPoi(const std::shared_ptr<PoiInstance> &poi);

    void addDweller(ServerNetworkHandler &owner, DwellerRole role, int64_t id);

    void removeDweller(DwellerRole role, int64_t id);

    void markDwellerSeen(DwellerRole role, int64_t id);

    bool hasDweller(DwellerRole role, int64_t id) const;

    size_t getInhabitantCount() const { return mRoles[(size_t) DwellerRole::Inhabitant].size(); }

    bool isInitialized() const { return mInitialized; }

    int32_t getTickCount() const { return mTickCount; }

    bool hasUnclaimedBed() const { return !mUnclaimed[(size_t) PoiType::Bed].empty(); }

    /** Unclaimed beds plus the beds claimed by inhabitants. */
    size_t countBeds() const;

    /** Every point of interest the village holds, claimed or not. */
    std::vector<std::shared_ptr<PoiInstance>> getPois() const;

    void tick(ServerNetworkHandler &owner, int64_t currentTick);

private:
    struct Inhabitant {
        int64_t mId;
        std::array<std::weak_ptr<PoiInstance>, POI_TYPE_COUNT> mPois;
    };

    struct DwellerEntry {
        int64_t mId;
        int64_t mLastSeenTick;
    };

    Inhabitant *_findInhabitant(int64_t id);

    Inhabitant &_findOrAddInhabitant(int64_t id);

    void _releaseInhabitant(int64_t id);

    void _claim(ServerNetworkHandler &owner, int64_t id, const std::shared_ptr<PoiInstance> &poi);

    void _claimNext(ServerNetworkHandler &owner, int64_t id, std::vector<std::shared_ptr<PoiInstance>> &queue);

    void _tickDwellerTimeouts(ServerNetworkHandler &owner);

    void _recalculateBounds();

    std::array<std::vector<std::shared_ptr<PoiInstance>>, POI_TYPE_COUNT> mUnclaimed;
    std::vector<Inhabitant> mInhabitants;
    std::array<std::vector<DwellerEntry>, ROLE_COUNT> mRoles;
    AxisAlignedBB mBounds;
    int64_t mLastTick = 0;
    int32_t mTickCount = 0;
    bool mInitialized = false;
};
