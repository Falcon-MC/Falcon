#pragma once

#include "World/Level/Map/MapItemSavedData.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class LevelStorage;

// MapDataManager: map_<id> records read on first use and unloaded once saved and left unused; main thread only
class MapDataManager {
public:
    using IdAllocator = std::function<int64_t()>;

    MapDataManager() = default;

    MapDataManager(MapDataManager &&) noexcept = default;

    MapDataManager &operator=(MapDataManager &&) noexcept = default;

    // sub_141ED0DD0 behind the cache; nullptr for a map storage does not hold
    MapItemSavedData *getMap(LevelStorage *storage, int64_t mapId);

    MapItemSavedData *findLoaded(int64_t mapId);

    // sub_141ED1C40: a blank map that replaces any loaded one
    MapItemSavedData &createMap(int64_t mapId);

    // sub_141ECEF80 with a new id: all five scales at once, each the parent of the one below
    MapItemSavedData &createMapSavedData(const IdAllocator &allocate, const Vector3f &position, int32_t dimension,
                                         int8_t scale);

    // sub_141ECFD30: the parent the next zoom level shows, recentred on the child unless it was already scaled
    int64_t expandMap(LevelStorage *storage, int64_t mapId, bool alreadyScaled);

    // sub_14101FB00
    MapItemSavedData *copyAndLockMap(LevelStorage *storage, int64_t originalId, int64_t newId);

    // the map's id followed by its parents while their scale is at most 3, for the creation packet
    std::vector<int64_t> getMapChain(LevelStorage *storage, MapItemSavedData &data);

    void saveDirty(LevelStorage &storage);

    // forgets the map in memory and in storage; false when storage kept it
    bool removeMap(LevelStorage *storage, int64_t mapId);

    // drops saved maps no lookup touched since the last call and no frame shows; storage reloads them on demand
    void unloadUnused();

    template <typename Function>
    void forEachLoaded(Function &&function) {
        for (auto &entry: mMaps)
            function(*entry.second.mData);
    }

private:
    struct Entry {
        std::unique_ptr<MapItemSavedData> mData;
        bool mUsed = true;
    };

    std::unordered_map<int64_t, Entry> mMaps;
    // ids storage lacks, so requests for unknown maps read the database once
    std::unordered_set<int64_t> mMissing;
};
