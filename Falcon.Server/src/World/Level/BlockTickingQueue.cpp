#include "World/Level/BlockTickingQueue.h"

#include "World/Level/Chunk/LevelChunk.h"

#include <utility>

namespace {
    bool isLater(const BlockTickingQueue::Entry &left, const BlockTickingQueue::Entry &right) {
        if (left.due != right.due)
            return left.due > right.due;
        return left.priority > right.priority;
    }
}

void BlockTickingQueue::moveStateFrom(BlockTickingQueue &&other) {
    if (this == &other)
        return;

    mQueues = std::move(other.mQueues);
    mPending = std::move(other.mPending);
    mTick = other.mTick;
    mScheduled = other.mScheduled;
    mLastProcessed = other.mLastProcessed;
}

bool BlockTickingQueue::_isBefore(int64_t left, int64_t right) {
    const int32_t leftZ = (int32_t) (left & 0xffffffff);
    const int32_t rightZ = (int32_t) (right & 0xffffffff);
    if (leftZ != rightZ)
        return leftZ < rightZ;
    return (int32_t) (left >> 32) < (int32_t) (right >> 32);
}

BlockTickingQueue::Queue &BlockTickingQueue::_queueAt(int32_t x, int32_t z) {
    const int64_t key = column(x >> 4, z >> 4);
    auto found = mQueues.find(key);
    if (found != mQueues.end())
        return found->second;

    // a chunk still waiting for its turn this tick lags one tick behind, as its queue would in the game
    Queue &queue = mQueues[key];
    queue.tick = mTicking && _isBefore(mCurrentColumn, key) ? mTick - 1 : mTick;
    return queue;
}

void BlockTickingQueue::_track(const Entry &entry) {
    ++mPending[Key{entry.x, entry.y, entry.z, entry.block, entry.due}];
    ++mScheduled;
}

void BlockTickingQueue::_untrack(const Entry &entry) {
    auto pending = mPending.find(Key{entry.x, entry.y, entry.z, entry.block, entry.due});
    if (pending != mPending.end() && --pending->second == 0)
        mPending.erase(pending);
    --mScheduled;
}

void BlockTickingQueue::_pushHeap(std::vector<Entry> &heap, const Entry &entry) {
    heap.push_back(entry);
    size_t hole = heap.size() - 1;
    while (hole > 0) {
        const size_t parent = (hole - 1) >> 1;
        if (!isLater(heap[parent], entry))
            break;
        heap[hole] = heap[parent];
        hole = parent;
    }
    heap[hole] = entry;
}

// _Pop_heap_hole_by_index: the hole walks down to a leaf, then the last element sifts up from it (sub_143838ED0)
BlockTickingQueue::Entry BlockTickingQueue::_popHeap(std::vector<Entry> &heap) {
    const Entry top = heap.front();
    const Entry last = heap.back();
    heap.pop_back();
    const ptrdiff_t count = (ptrdiff_t) heap.size();
    if (count == 0)
        return top;

    ptrdiff_t hole = 0;
    const ptrdiff_t maxNonLeaf = (count - 1) >> 1;
    while (hole < maxNonLeaf) {
        ptrdiff_t child = 2 * hole + 2;
        if (isLater(heap[child], heap[child - 1]))
            --child;
        heap[hole] = heap[child];
        hole = child;
    }
    if ((count & 1) == 0 && hole == maxNonLeaf) {
        heap[hole] = heap[count - 1];
        hole = count - 1;
    }
    while (hole > 0) {
        const ptrdiff_t parent = (hole - 1) >> 1;
        if (!isLater(heap[parent], last))
            break;
        heap[hole] = heap[parent];
        hole = parent;
    }
    heap[hole] = last;
    return top;
}

void BlockTickingQueue::beginTick(int64_t levelTick) {
    mTick = levelTick;
    mLastProcessed = 0;
    mTicking = true;
}

void BlockTickingQueue::endTick() {
    mTicking = false;
}

void BlockTickingQueue::add(const Vector3i &position, const Block *block, int32_t delay, int32_t priority) {
    if (position.y < LevelChunk::MIN_Y || position.y > LevelChunk::MAX_Y)
        return;

    Queue &queue = _queueAt(position.x, position.z);
    const Entry entry{position.x, position.y, position.z, priority, queue.tick + delay, block};
    _pushHeap(queue.heap, entry);
    _track(entry);
}

bool BlockTickingQueue::isPending(const Vector3i &position, const Block *block, int32_t delay) const {
    auto found = mQueues.find(column(position.x >> 4, position.z >> 4));
    if (found == mQueues.end())
        return false;

    const Key key{position.x, position.y, position.z, block, found->second.tick + delay};
    return mPending.find(key) != mPending.end();
}

int64_t BlockTickingQueue::getColumnTick(int32_t chunkX, int32_t chunkZ) const {
    auto found = mQueues.find(column(chunkX, chunkZ));
    return found == mQueues.end() ? mTick : found->second.tick;
}

const std::vector<BlockTickingQueue::Entry> *BlockTickingQueue::getColumnEntries(int32_t chunkX,
                                                                                int32_t chunkZ) const {
    auto found = mQueues.find(column(chunkX, chunkZ));
    return found == mQueues.end() || found->second.heap.empty() ? nullptr : &found->second.heap;
}

void BlockTickingQueue::loadColumn(int32_t chunkX, int32_t chunkZ, int64_t savedTick,
                                   const std::vector<Entry> &entries) {
    dropColumn(chunkX, chunkZ);
    for (const Entry &saved: entries) {
        if (saved.x >> 4 != chunkX || saved.z >> 4 != chunkZ)
            continue;
        Entry entry = saved;
        entry.due = saved.due - savedTick + mTick;
        Queue &queue = _queueAt(entry.x, entry.z);
        _pushHeap(queue.heap, entry);
        _track(entry);
    }
}

void BlockTickingQueue::dropColumn(int32_t chunkX, int32_t chunkZ) {
    auto found = mQueues.find(column(chunkX, chunkZ));
    if (found == mQueues.end())
        return;
    for (const Entry &entry: found->second.heap)
        _untrack(entry);
    mQueues.erase(found);
}
