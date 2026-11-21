#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// neighbor changes of blocks with minecraft:neighbor_change, gathered over a tick and fired once a block
// one change per direction: before its first change, after its last
class NeighborChangeQueue {
public:
    struct Change {
        int32_t direction = 0;
        BlockState previous;
        BlockState current;
    };

    struct Entry {
        Vector3i position;
        std::string blockType;
        std::vector<Change> changes;
    };

    // `direction` goes from the listening block at `position` to the neighbor that changed
    void record(const Vector3i &position, const std::string &blockType, int32_t direction, const BlockState &previous,
                const BlockState &current);

    bool empty() const {
        return mEntries.empty();
    }

    // the blocks in the order their first change came; the queue is empty after
    std::vector<Entry> take();

    void clear();

private:
    static int64_t _key(const Vector3i &position);

    std::vector<Entry> mEntries;
    std::unordered_map<int64_t, size_t> mIndex;
};
