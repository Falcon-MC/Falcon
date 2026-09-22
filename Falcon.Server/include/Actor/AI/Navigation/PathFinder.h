#pragma once

#include "Actor/AI/Navigation/WalkNodeEvaluator.h"
#include "Core/Math/Vector3f.h"

#include <array>
#include <cstdint>
#include <vector>

class Level;
class MobActor;
class Path;

class PathFinder {
public:
    static constexpr int32_t MAX_EXPANSIONS = 100;
    static constexpr int32_t MAX_SEARCHES_PER_TICK = 4;

    static PathFinder &get();

    static bool tryReserveSearch(int64_t tick);

    bool findPath(Level &level, const MobActor &mob, const Vector3f &target, Path &path);

private:
    struct Node {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        int32_t mG;
        int32_t mH;
        int32_t mParent;
        int32_t mHeapIndex;
        bool mClosed;
    };

    static constexpr uint32_t INDEX_CAPACITY = 2048;
    static constexpr int32_t NOT_IN_HEAP = -1;

    int32_t _findNode(int32_t x, int32_t y, int32_t z) const;

    void _offer(int32_t x, int32_t y, int32_t z, int32_t g, int32_t parent);

    void _expand(int32_t nodeIndex);

    int32_t _heuristic(int32_t x, int32_t y, int32_t z) const;

    void _heapPush(int32_t nodeIndex);

    int32_t _heapPop();

    void _siftUp(int32_t heapIndex);

    void _siftDown(int32_t heapIndex);

    bool _less(int32_t left, int32_t right) const;

    void _buildPath(int32_t endIndex, bool reachesTarget, const Vector3f &start, const Vector3f &target,
                    Path &path);

    WalkNodeEvaluator mEvaluator;
    std::vector<Node> mNodes;
    std::vector<int32_t> mHeap;
    std::vector<Vector3f> mChain;
    std::array<int64_t, INDEX_CAPACITY> mIndexKeys{};
    std::array<int32_t, INDEX_CAPACITY> mIndexValues{};
    std::array<uint32_t, INDEX_CAPACITY> mIndexStamps{};
    uint32_t mStamp = 0;
    int32_t mTargetX = 0;
    int32_t mTargetY = 0;
    int32_t mTargetZ = 0;
};
