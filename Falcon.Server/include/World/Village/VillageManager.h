#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/PoiScanner.h"
#include "World/Village/Village.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Level;
class MobActor;
class ServerNetworkHandler;

/**
 * The villages of one level, the queue of dwellers waiting for a point of interest scan and the points of interest
 * already owned by a village. The manager holds the only strong reference to each of those points. Main-thread only.
 */
class VillageManager {
public:
    /** Asks for a scan around a dweller. Ignored when the dweller is queued already or 64 are waiting. */
    void requestScan(int64_t dwellerId);

    /** The level part of the tick: removes dead villages, advances the dweller scan and files what it found. */
    void tick(ServerNetworkHandler &owner, Level &level, int64_t currentTick);

    /**
     * Ticks the villages around a player, whose box grown by half the chunk tick range holds the player, then their
     * raids. A raid of a village out of range only advances once the village is lost or the raid is over.
     */
    void tickVillagesNear(ServerNetworkHandler &owner, Level &level, int64_t currentTick, const Vector3f &position,
                          int32_t chunkTickRange);

    /** Whether a village owns a point of interest of that type and name at position. */
    bool isRegistered(PoiType type, const Vector3i &position, const std::string &name) const;

    /** Forgets a point of interest, which expires every claim and queue entry referring to it. */
    void releasePoi(const PoiInstance &poi);

    std::shared_ptr<Village> getClosestVillage(const Vector3i &position, int32_t radius) const;

    /** The village with a raid whose centre is closest to position, at any distance. */
    std::shared_ptr<Village> getClosestRaidingVillage(const Vector3i &position) const;

    /** Like getClosestVillage, but a village with a free bed wins over one without. */
    std::shared_ptr<Village> getClosestVillageForDweller(const Vector3i &position, int32_t radius) const;

    /** The village that lists the dweller with that role, if any. */
    std::shared_ptr<Village> getDwellerVillage(DwellerRole role, int64_t id) const;

    static MobActor *findMob(ServerNetworkHandler &owner, int64_t uniqueId);

private:
    struct PositionHash {
        size_t operator()(const Vector3i &position) const;
    };

    using PoiMap = std::unordered_map<Vector3i, std::shared_ptr<PoiInstance>, PositionHash>;

    void _removeVillages();

    void _scanNextDweller(ServerNetworkHandler &owner, Level &level);

    void _popScanRequest();

    bool _isKnownPoi(const Vector3i &position) const;

    void _registerPoi(const std::shared_ptr<PoiInstance> &poi);

    void _fileBed(const std::shared_ptr<PoiInstance> &poi);

    void _fileOther(const std::shared_ptr<PoiInstance> &poi);

    std::vector<std::shared_ptr<Village>> mVillages;
    std::array<PoiMap, Village::POI_TYPE_COUNT> mPois;
    std::deque<int64_t> mScanQueue;
    PoiScanner mScanner;
    std::vector<std::shared_ptr<PoiInstance>> mFound;
    int64_t mLastTick = 0;
};
