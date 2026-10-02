#include "Actor/AI/Navigation/PathFinder.h"

#include "Actor/AI/Navigation/Path.h"
#include "Actor/Mob/MobActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace {
    const int32_t DIRECT_MOVE_COST = 10;
    const int32_t OBLIQUE_MOVE_COST = 14;
    const int32_t MAX_SMOOTH_LOOKAHEAD = 8;
    const float BLOCK_CENTER = 0.5f;

    const int32_t ORTHOGONAL_X[] = {1, 0, -1, 0};
    const int32_t ORTHOGONAL_Z[] = {0, 1, 0, -1};

    int64_t packPosition(int32_t x, int32_t y, int32_t z) {
        return ((int64_t) (x & 0x3FFFFFF) << 38) | ((int64_t) (z & 0x3FFFFFF) << 12) | (int64_t) (y & 0xFFF);
    }

    uint32_t hashPosition(int64_t key) {
        uint64_t value = (uint64_t) key * 0x9E3779B97F4A7C15ull;
        return (uint32_t) (value >> 32);
    }
}

PathFinder &PathFinder::get() {
    static PathFinder finder;
    return finder;
}

bool PathFinder::tryReserveSearch(int64_t tick) {
    static int64_t currentTick = -1;
    static int32_t searches = 0;

    if (tick != currentTick) {
        currentTick = tick;
        searches = 0;
    }

    if (searches >= MAX_SEARCHES_PER_TICK)
        return false;

    searches++;
    return true;
}

bool PathFinder::findPath(Level &level, const MobActor &mob, const Vector3f &target, const PathOptions &options,
                          Path &path) {
    path.clear();
    mNodes.clear();
    mHeap.clear();
    if (mNodes.capacity() < INDEX_CAPACITY / 2) {
        mNodes.reserve(INDEX_CAPACITY / 2);
        mHeap.reserve(INDEX_CAPACITY / 2);
    }

    mStamp++;
    if (mStamp == 0) {
        mIndexStamps.fill(0);
        mStamp = 1;
    }

    const Vector3f position = mob.getPosition();
    const ActorSize size = mob.getSize();
    const bool inWater = LiquidBlocksFetch::at(level, position).water;
    mEvaluator.prepare(level, size.mWidth, size.mHeight, position, inWater, options);

    mTargetX = (int32_t) std::floor(target.x);
    mTargetY = (int32_t) std::floor(target.y);
    mTargetZ = (int32_t) std::floor(target.z);

    _offer((int32_t) std::floor(position.x), (int32_t) std::floor(position.y), (int32_t) std::floor(position.z), 0,
           -1);
    int32_t current = _heapPop();
    mNodes[current].mClosed = true;

    int32_t budget = MAX_EXPANSIONS;
    bool reached = false;
    while (true) {
        const Node &node = mNodes[current];
        if (node.mX == mTargetX && node.mY == mTargetY && node.mZ == mTargetZ) {
            reached = true;
            break;
        }

        _expand(current);
        if (mHeap.empty() || budget-- <= 0)
            break;

        current = _heapPop();
        mNodes[current].mClosed = true;
    }

    int32_t end = current;
    if (!reached) {
        int64_t nearestDistance = INT64_MAX;
        for (int32_t index = 0; index < (int32_t) mNodes.size(); ++index) {
            const Node &node = mNodes[index];
            if (!node.mClosed)
                continue;

            const int64_t dx = node.mX - mTargetX;
            const int64_t dy = node.mY - mTargetY;
            const int64_t dz = node.mZ - mTargetZ;
            const int64_t distance = dx * dx + dy * dy + dz * dz;
            if (distance < nearestDistance) {
                nearestDistance = distance;
                end = index;
            }
        }
    }

    _buildPath(end, reached, position, target, path);
    return !path.isEmpty();
}

int32_t PathFinder::_findNode(int32_t x, int32_t y, int32_t z) const {
    const int64_t key = packPosition(x, y, z);
    uint32_t slot = hashPosition(key) & (INDEX_CAPACITY - 1);

    for (uint32_t probe = 0; probe < INDEX_CAPACITY; ++probe) {
        if (mIndexStamps[slot] != mStamp)
            return -1;
        if (mIndexKeys[slot] == key)
            return mIndexValues[slot];
        slot = (slot + 1) & (INDEX_CAPACITY - 1);
    }
    return -1;
}

void PathFinder::_offer(int32_t x, int32_t y, int32_t z, int32_t g, int32_t parent) {
    const int32_t existing = _findNode(x, y, z);
    if (existing >= 0) {
        Node &node = mNodes[existing];
        if (node.mClosed || g >= node.mG)
            return;

        node.mG = g;
        node.mParent = parent;
        _siftUp(node.mHeapIndex);
        return;
    }

    if (mNodes.size() >= INDEX_CAPACITY / 2)
        return;

    const int32_t index = (int32_t) mNodes.size();
    mNodes.push_back({x, y, z, g, _heuristic(x, y, z), parent, NOT_IN_HEAP, false});

    const int64_t key = packPosition(x, y, z);
    uint32_t slot = hashPosition(key) & (INDEX_CAPACITY - 1);
    while (mIndexStamps[slot] == mStamp)
        slot = (slot + 1) & (INDEX_CAPACITY - 1);

    mIndexStamps[slot] = mStamp;
    mIndexKeys[slot] = key;
    mIndexValues[slot] = index;

    _heapPush(index);
}

void PathFinder::_expand(int32_t nodeIndex) {
    const Node node = mNodes[nodeIndex];

    const int32_t selfOffset = mEvaluator.availableOffset(node.mX, node.mY, node.mZ);
    if (selfOffset != WalkNodeEvaluator::NO_OFFSET && selfOffset != 0)
        _offer(node.mX, node.mY + selfOffset, node.mZ, node.mG, nodeIndex);

    bool open[4];
    for (int32_t direction = 0; direction < 4; ++direction) {
        const int32_t x = node.mX + ORTHOGONAL_X[direction];
        const int32_t z = node.mZ + ORTHOGONAL_Z[direction];
        const int32_t offset = mEvaluator.availableOffset(x, node.mY, z);
        open[direction] = offset != WalkNodeEvaluator::NO_OFFSET;
        if (!open[direction])
            continue;

        const int32_t y = node.mY + offset;
        _offer(x, y, z, node.mG + DIRECT_MOVE_COST + mEvaluator.extraCost(x, y, z), nodeIndex);
    }

    for (int32_t direction = 0; direction < 4; ++direction) {
        const int32_t next = (direction + 1) & 3;
        if (!open[direction] || !open[next])
            continue;

        const int32_t x = node.mX + ORTHOGONAL_X[direction] + ORTHOGONAL_X[next];
        const int32_t z = node.mZ + ORTHOGONAL_Z[direction] + ORTHOGONAL_Z[next];
        const int32_t offset = mEvaluator.availableOffset(x, node.mY, z);
        if (offset != 0 && (offset == WalkNodeEvaluator::NO_OFFSET || !mEvaluator.isInWater()))
            continue;

        const int32_t y = node.mY + offset;
        _offer(x, y, z, node.mG + OBLIQUE_MOVE_COST + mEvaluator.extraCost(x, y, z), nodeIndex);
    }
}

int32_t PathFinder::_heuristic(int32_t x, int32_t y, int32_t z) const {
    const int32_t dx = std::abs(mTargetX - x);
    const int32_t dz = std::abs(mTargetZ - z);
    const int32_t diagonal = std::min(dx, dz);
    const int32_t straight = std::max(dx, dz) - diagonal;
    return diagonal * OBLIQUE_MOVE_COST + straight * DIRECT_MOVE_COST + std::abs(mTargetY - y) * DIRECT_MOVE_COST;
}

void PathFinder::_heapPush(int32_t nodeIndex) {
    mNodes[nodeIndex].mHeapIndex = (int32_t) mHeap.size();
    mHeap.push_back(nodeIndex);
    _siftUp(mNodes[nodeIndex].mHeapIndex);
}

int32_t PathFinder::_heapPop() {
    const int32_t top = mHeap.front();
    const int32_t last = mHeap.back();
    mHeap.pop_back();
    mNodes[top].mHeapIndex = NOT_IN_HEAP;

    if (!mHeap.empty()) {
        mHeap[0] = last;
        mNodes[last].mHeapIndex = 0;
        _siftDown(0);
    }
    return top;
}

void PathFinder::_siftUp(int32_t heapIndex) {
    while (heapIndex > 0) {
        const int32_t parent = (heapIndex - 1) / 2;
        if (!_less(mHeap[heapIndex], mHeap[parent]))
            return;

        std::swap(mHeap[heapIndex], mHeap[parent]);
        mNodes[mHeap[heapIndex]].mHeapIndex = heapIndex;
        mNodes[mHeap[parent]].mHeapIndex = parent;
        heapIndex = parent;
    }
}

void PathFinder::_siftDown(int32_t heapIndex) {
    const int32_t size = (int32_t) mHeap.size();
    while (true) {
        const int32_t left = heapIndex * 2 + 1;
        const int32_t right = left + 1;
        int32_t smallest = heapIndex;

        if (left < size && _less(mHeap[left], mHeap[smallest]))
            smallest = left;
        if (right < size && _less(mHeap[right], mHeap[smallest]))
            smallest = right;
        if (smallest == heapIndex)
            return;

        std::swap(mHeap[heapIndex], mHeap[smallest]);
        mNodes[mHeap[heapIndex]].mHeapIndex = heapIndex;
        mNodes[mHeap[smallest]].mHeapIndex = smallest;
        heapIndex = smallest;
    }
}

bool PathFinder::_less(int32_t left, int32_t right) const {
    const Node &a = mNodes[left];
    const Node &b = mNodes[right];
    const int32_t fa = a.mG + a.mH;
    const int32_t fb = b.mG + b.mH;
    return fa != fb ? fa < fb : a.mH < b.mH;
}

void PathFinder::_buildPath(int32_t endIndex, bool reachesTarget, const Vector3f &start, const Vector3f &target,
                            Path &path) {
    mChain.clear();
    for (int32_t index = endIndex; index > 0; index = mNodes[index].mParent) {
        const Node &node = mNodes[index];
        mChain.emplace_back((float) node.mX + BLOCK_CENTER, (float) node.mY, (float) node.mZ + BLOCK_CENTER);
    }
    std::reverse(mChain.begin(), mChain.end());

    const Vector3f &last = mChain.empty() ? start : mChain.back();
    if (reachesTarget && !mEvaluator.hasBarrier(last, target))
        mChain.push_back(target);

    Vector3f anchor = start;
    const int32_t count = (int32_t) mChain.size();
    int32_t index = 0;
    while (index < count) {
        int32_t furthest = index;
        const int32_t limit = std::min(count - 1, index + MAX_SMOOTH_LOOKAHEAD);
        while (furthest < limit && !mEvaluator.hasBarrier(anchor, mChain[furthest + 1]))
            furthest++;

        path.add(mChain[furthest]);
        anchor = mChain[furthest];
        index = furthest + 1;
    }

    path.setReachesTarget(reachesTarget);
}
