#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

class Level;

// the block points of interest of one dimension under the Poi experiment; a loaded block of a poi type is one,
// a temporary one lasts until its chunk reloads
class PoiManager {
public:
    struct Entry {
        Vector3i mPosition;
        uint16_t mType = 0;
        // still claimable
        uint8_t mTickets = 0;
    };

    // false from the visitor stops the walk; min and max are inclusive (sub_14B8C2BF0, 26.60)
    void forEachInBox(Level &level, const Vector3i &min, const Vector3i &max,
                      const std::function<bool(Entry &)> &visitor);

    // nullptr without a poi there or in an unloaded chunk
    Entry *at(Level &level, const Vector3i &position);

    // ScriptBlockManager::addTemporary (sub_14B959AB0, 26.60): every instance starts with all its type's tickets
    void addTemporary(Level &level, const Vector3i &position, uint16_t type);

    // PoiRegion::release (sub_1485096E0, 26.60): one ticket back while below the type's count
    bool release(Level &level, const Vector3i &position);

private:
    struct ChunkPois {
        uint64_t mInstance = 0;
        uint32_t mRevision = 0;
        bool mScanned = false;
        std::vector<Entry> mEntries;
        std::vector<Entry> mTemporary;
    };

    ChunkPois *_ensure(Level &level, int32_t chunkX, int32_t chunkZ);

    void _scan(Level &level, int32_t chunkX, int32_t chunkZ, ChunkPois &pois);

    static int64_t _key(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) ^ (uint32_t) chunkZ;
    }

    std::unordered_map<int64_t, ChunkPois> mChunks;
};
