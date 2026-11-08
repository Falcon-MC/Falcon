#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// the MapInfoRequestPacket ids a player waits on: each answer costs a storage read and ~64 KB; a few drain per tick,
// a full queue drops the rest
class MapRequestQueue {
public:
    static constexpr size_t CAPACITY = 256;
    static constexpr size_t PER_TICK = 4;

    // false for an id already waiting or a full queue
    bool push(int64_t mapId);

    // up to PER_TICK ids, oldest first
    std::vector<int64_t> takeDue();

    size_t size() const {
        return mIds.size();
    }

private:
    std::vector<int64_t> mIds;
};
