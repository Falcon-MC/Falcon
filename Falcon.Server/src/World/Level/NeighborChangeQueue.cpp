#include "World/Level/NeighborChangeQueue.h"

void NeighborChangeQueue::record(const Vector3i &position, const std::string &blockType, int32_t direction,
                                 const BlockState &previous, const BlockState &current) {
    const auto inserted = mIndex.emplace(_key(position), mEntries.size());
    if (inserted.second) {
        mEntries.push_back(Entry());
        mEntries.back().position = position;
        mEntries.back().blockType = blockType;
    }

    Entry &entry = mEntries[inserted.first->second];
    // another block took the place within the tick: its changes start over
    if (entry.blockType != blockType) {
        entry.blockType = blockType;
        entry.changes.clear();
    }
    for (Change &change: entry.changes) {
        if (change.direction == direction) {
            change.current = current;
            return;
        }
    }
    entry.changes.push_back(Change{direction, previous, current});
}

std::vector<NeighborChangeQueue::Entry> NeighborChangeQueue::take() {
    std::vector<Entry> taken;
    taken.swap(mEntries);
    mIndex.clear();
    return taken;
}

void NeighborChangeQueue::clear() {
    mEntries.clear();
    mIndex.clear();
}

int64_t NeighborChangeQueue::_key(const Vector3i &position) {
    // 26 bits x and z, 12 bits y: the whole build height and the world border fit
    return ((int64_t) (position.x & 0x3FFFFFF) << 38) | ((int64_t) (position.z & 0x3FFFFFF) << 12)
           | (int64_t) (position.y & 0xFFF);
}
