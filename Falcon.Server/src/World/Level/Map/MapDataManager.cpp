#include "World/Level/Map/MapDataManager.h"

#include "Core/NBT/Tag.h"
#include "World/Level/Storage/LevelStorage.h"

#include <algorithm>

namespace {
    const int32_t SCALE_COUNT = MapItemSavedData::MAX_SCALE + 1;
    const size_t MISSING_CACHE_LIMIT = 4096;
}

MapItemSavedData *MapDataManager::findLoaded(int64_t mapId) {
    const auto it = mMaps.find(mapId);
    return it == mMaps.end() ? nullptr : it->second.get();
}

MapItemSavedData *MapDataManager::getMap(LevelStorage *storage, int64_t mapId) {
    if (MapItemSavedData *loaded = findLoaded(mapId))
        return loaded;

    if (storage == nullptr || mapId == MapItemSavedData::INVALID_ID || mMissing.count(mapId) != 0)
        return nullptr;

    Tag data;
    if (!storage->loadMapData(mapId, data)) {
        if (mMissing.size() >= MISSING_CACHE_LIMIT)
            mMissing.clear();
        mMissing.insert(mapId);
        return nullptr;
    }

    std::unique_ptr<MapItemSavedData> map(new MapItemSavedData(mapId));
    map->load(data);
    MapItemSavedData *result = map.get();
    mMaps[mapId] = std::move(map);
    return result;
}

MapItemSavedData &MapDataManager::createMap(int64_t mapId) {
    mMissing.erase(mapId);
    std::unique_ptr<MapItemSavedData> map(new MapItemSavedData(mapId));
    map->markDirty();
    MapItemSavedData &result = *map;
    mMaps[mapId] = std::move(map);
    return result;
}

MapItemSavedData &MapDataManager::createMapSavedData(const IdAllocator &allocate, const Vector3f &position,
                                                     int32_t dimension, int8_t scale) {
    int64_t ids[SCALE_COUNT];
    for (int64_t &id: ids)
        id = allocate();

    // sub_141ECF1A0 walks from the widest scale down, so each map knows its parent when created
    int64_t parent = MapItemSavedData::INVALID_ID;
    for (int32_t level = SCALE_COUNT - 1; level >= 0; --level) {
        MapItemSavedData &map = createMap(ids[level]);
        map.setParentMapId(parent);
        map.setScale((int8_t) level);
        map.setOrigin(position, (int8_t) level, dimension);
        parent = ids[level];
    }

    const int32_t index = scale > 0 && scale < SCALE_COUNT ? scale : 0;
    return *findLoaded(ids[index]);
}

int64_t MapDataManager::expandMap(LevelStorage *storage, int64_t mapId, bool alreadyScaled) {
    MapItemSavedData *child = getMap(storage, mapId);
    if (child == nullptr)
        return mapId;

    MapItemSavedData *parent = getMap(storage, child->getParentMapId());
    if (parent == nullptr)
        return mapId;

    if (!alreadyScaled && child->getScale() < MapItemSavedData::MAX_SCALE) {
        const Vector3i &center = child->getOrigin();
        parent->setOrigin(Vector3f((float) center.x, (float) center.y, (float) center.z),
                          (int8_t) (child->getScale() + 1), child->getDimension());
    }
    return parent->getMapId();
}

MapItemSavedData *MapDataManager::copyAndLockMap(LevelStorage *storage, int64_t originalId, int64_t newId) {
    MapItemSavedData *original = getMap(storage, originalId);
    if (original == nullptr)
        return nullptr;

    MapItemSavedData &copy = createMap(newId);
    copy.copyFrom(*original);
    copy.setLocked(true);
    copy.markDirty();
    return &copy;
}

std::vector<int64_t> MapDataManager::getMapChain(LevelStorage *storage, MapItemSavedData &data) {
    std::vector<int64_t> ids(1, data.getMapId());
    MapItemSavedData *current = &data;
    while (current != nullptr && current->getScale() <= 3) {
        current = getMap(storage, current->getParentMapId());
        if (current != nullptr)
            ids.push_back(current->getMapId());
    }
    return ids;
}

void MapDataManager::saveDirty(LevelStorage &storage) {
    for (auto &entry: mMaps) {
        MapItemSavedData &map = *entry.second;
        if (!map.isDirtyForSave())
            continue;

        Tag data = Tag::ofCompound();
        map.save(data);
        if (storage.saveMapData(map.getMapId(), data))
            map.clearDirtyForSave();
    }
}
