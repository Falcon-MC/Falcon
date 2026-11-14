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
class Tag;
class ServerNetworkHandler;

// holds the only strong reference to each poi; main thread only
class VillageManager {
public:
    // ignored when already queued or 64 are waiting
    void requestScan(int64_t dwellerId);

    // every village's records in the level's database; the game keys them by dimension name
    void save(Level &level) const;

    // the villages the level's database holds, once its storage is open
    void load(Level &level, int64_t currentTick);

    // removes dead villages, advances the dweller scan, files its findings
    void tick(ServerNetworkHandler &owner, Level &level, int64_t currentTick);

    // villages whose box grown by half the chunk tick range holds the player, then their raids;
    // an out-of-range village's raid only advances once the village is lost or the raid is over
    void tickVillagesNear(ServerNetworkHandler &owner, Level &level, int64_t currentTick, const Vector3f &position,
                          int32_t chunkTickRange);

    bool isRegistered(PoiType type, const Vector3i &position, const std::string &name) const;

    // expires every claim and queue entry referring to it
    void releasePoi(const PoiInstance &poi);

    std::shared_ptr<Village> getClosestVillage(const Vector3i &position, int32_t radius) const;

    // raid centre closest to position, any distance
    std::shared_ptr<Village> getClosestRaidingVillage(const Vector3i &position) const;

    // a village with a free bed wins over one without
    std::shared_ptr<Village> getClosestVillageForDweller(const Vector3i &position, int32_t radius) const;

    std::shared_ptr<Village> getDwellerVillage(DwellerRole role, int64_t id) const;

    static MobActor *findMob(ServerNetworkHandler &owner, int64_t uniqueId);

private:
    struct PositionHash {
        size_t operator()(const Vector3i &position) const;
    };

    using PoiMap = std::unordered_map<Vector3i, std::shared_ptr<PoiInstance>, PositionHash>;

    void _removeVillages(Level &level);

    static std::string _keyPrefix(const Level &level);

    void _scanNextDweller(ServerNetworkHandler &owner, Level &level);

    void _popScanRequest();

    bool _isKnownPoi(const Vector3i &position) const;

    void _registerPoi(const std::shared_ptr<PoiInstance> &poi);

    // the instance at the saved position, shared with the villagers that also hold it
    std::shared_ptr<PoiInstance> _restorePoi(const Tag &instance);

    void _fileBed(const std::shared_ptr<PoiInstance> &poi);

    void _fileOther(const std::shared_ptr<PoiInstance> &poi);

    std::vector<std::shared_ptr<Village>> mVillages;
    std::array<PoiMap, Village::POI_TYPE_COUNT> mPois;
    std::deque<int64_t> mScanQueue;
    PoiScanner mScanner;
    std::vector<std::shared_ptr<PoiInstance>> mFound;
    int64_t mLastTick = 0;
};
