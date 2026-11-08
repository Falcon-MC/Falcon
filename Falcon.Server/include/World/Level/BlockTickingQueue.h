#pragma once

#include "Core/Math/Vector3i.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class Block;

// one queue per chunk, as LevelChunk keeps its BlockTickingQueue (sub_14381AE20, sub_14381B7E0); entries are
// never merged: a block may hold several pending ticks
class BlockTickingQueue {
public:
    struct Entry {
        int32_t x;
        int32_t y;
        int32_t z;
        int32_t priority;
        int64_t due;
        const Block *block;
    };

    static constexpr size_t MAX_TICKS_PER_CHUNK = 100;

    void moveStateFrom(BlockTickingQueue &&other);

    // `block` is the block the tick fires for: it runs only if that block is still there
    void add(const Vector3i &position, const Block *block, int32_t delay, int32_t priority = 0);

    // the check neighborChanged makes before adding (sub_149C3DEA0): same block, same due tick
    bool isPending(const Vector3i &position, const Block *block, int32_t delay) const;

    // one chunk's turn in a level tick, columns taken in z then x order; `handler(position, block)` per tick
    // due, at most 100; false when none ran (sub_14381B7E0)
    template<typename Handler, typename Loaded>
    bool tickColumn(int64_t levelTick, int64_t column, Handler &&handler, Loaded &&isLoaded);

    // a level tick starts or ends: chunks not yet reached lag one tick behind
    void beginTick(int64_t levelTick);
    void endTick();

    // the column's entries with times relative to its own clock, in heap order, for the PendingTicks record
    int64_t getColumnTick(int32_t chunkX, int32_t chunkZ) const;
    const std::vector<Entry> *getColumnEntries(int32_t chunkX, int32_t chunkZ) const;

    // entries read back are rebased: a saved `currentTick` becomes the level's tick
    void loadColumn(int32_t chunkX, int32_t chunkZ, int64_t savedTick, const std::vector<Entry> &entries);

    void dropColumn(int32_t chunkX, int32_t chunkZ);

    size_t getScheduledCount() const {
        return mScheduled;
    }

    size_t getLastProcessedCount() const {
        return mLastProcessed;
    }

    static int64_t column(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    }

private:
    struct Queue {
        int64_t tick = 0;
        std::vector<Entry> heap;
    };

    struct Key {
        int32_t x;
        int32_t y;
        int32_t z;
        const Block *block;
        int64_t due;

        bool operator==(const Key &other) const {
            return x == other.x && y == other.y && z == other.z && block == other.block && due == other.due;
        }
    };

    struct KeyHash {
        size_t operator()(const Key &key) const {
            uint64_t value = (uint64_t) (uint32_t) key.x * 0x9E3779B97F4A7C15ull;
            value ^= (uint64_t) (uint32_t) key.z * 0xC2B2AE3D27D4EB4Full + (value << 6) + (value >> 2);
            value ^= ((uint64_t) (uint32_t) key.y ^ (uint64_t) (uintptr_t) key.block) + (value << 6) + (value >> 2);
            value ^= (uint64_t) key.due * 0x165667B19E3779F9ull + (value << 6) + (value >> 2);
            return (size_t) value;
        }
    };

    static bool _isBefore(int64_t left, int64_t right);

    Queue &_queueAt(int32_t x, int32_t z);

    void _track(const Entry &entry);
    void _untrack(const Entry &entry);

    // std::priority_queue under MSVC: push_heap and pop_heap break ties between equal (due, priority) entries
    static void _pushHeap(std::vector<Entry> &heap, const Entry &entry);
    static Entry _popHeap(std::vector<Entry> &heap);

    std::unordered_map<int64_t, Queue> mQueues;
    // entries still in a queue heap, counted: the same tick may be queued twice
    std::unordered_map<Key, uint32_t, KeyHash> mPending;
    std::vector<Entry> mActive;
    bool mTicking = false;
    int64_t mCurrentColumn = 0;
    int64_t mTick = 0;
    size_t mScheduled = 0;
    size_t mLastProcessed = 0;
};

template<typename Handler, typename Loaded>
bool BlockTickingQueue::tickColumn(int64_t levelTick, int64_t key, Handler &&handler, Loaded &&isLoaded) {
    mCurrentColumn = key;
    auto found = mQueues.find(key);
    if (found == mQueues.end())
        return false;

    // unordered_map nodes stay put: the reference survives queues added by the handler
    Queue &queue = found->second;
    if (queue.tick == levelTick)
        return false;
    queue.tick = levelTick;

    const size_t limit = queue.heap.size() < MAX_TICKS_PER_CHUNK ? queue.heap.size() : MAX_TICKS_PER_CHUNK;
    std::vector<Entry> active;
    active.swap(mActive);
    active.clear();
    while (!queue.heap.empty() && active.size() < limit && queue.heap.front().due <= levelTick) {
        const Entry entry = _popHeap(queue.heap);
        _untrack(entry);
        _pushHeap(active, entry);
    }

    const bool ran = !active.empty();
    while (!active.empty()) {
        const Entry entry = _popHeap(active);
        bool surrounded = true;
        for (int32_t chunkX = (entry.x - 8) >> 4; chunkX <= (entry.x + 8) >> 4 && surrounded; ++chunkX) {
            for (int32_t chunkZ = (entry.z - 8) >> 4; chunkZ <= (entry.z + 8) >> 4; ++chunkZ) {
                if (!isLoaded(chunkX, chunkZ)) {
                    surrounded = false;
                    break;
                }
            }
        }

        const Vector3i position(entry.x, entry.y, entry.z);
        if (!surrounded) {
            // neighbours missing: the tick waits for the next one if its own chunk is there
            if (isLoaded(entry.x >> 4, entry.z >> 4))
                add(position, entry.block, 0, entry.priority);
            continue;
        }

        ++mLastProcessed;
        handler(position, entry.block);
    }
    active.swap(mActive);

    found = mQueues.find(key);
    if (found != mQueues.end() && found->second.heap.empty())
        mQueues.erase(found);
    return ran;
}
