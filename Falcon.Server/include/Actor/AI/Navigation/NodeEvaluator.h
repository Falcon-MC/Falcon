#pragma once

#include "Actor/AI/Navigation/PathOptions.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"

#include <array>
#include <cstdint>

class Level;

/**
 * Classifies block positions for the pathfinder. Ground modes (walk, climb) look for a block to stand on,
 * volumetric modes (swim, fly) accept any open position in their medium. Every lookup is cached for the
 * duration of one search because A* asks about the same blocks many times.
 */
class NodeEvaluator {
public:
    static constexpr int32_t NO_OFFSET = INT32_MIN;
    static constexpr int32_t MAX_DROP = 4;
    static constexpr int32_t LIQUID_EXTRA_COST = 20;
    static constexpr int32_t HAZARD_EXTRA_COST = 80;

    void prepare(Level &level, float width, float height, const Vector3f &start, bool inWater,
                 const PathOptions &options);

    const PathOptions &getOptions() const {
        return *mOptions;
    }

    bool isInWater() const {
        return mInWater;
    }

    /** Vertical offset from `feetY` to the nearest standable position in this column, or NO_OFFSET. */
    int32_t availableOffset(int32_t x, int32_t feetY, int32_t z);

    bool isStandable(int32_t x, int32_t y, int32_t z);

    /** A climber can hang at this position: the body fits and a solid block is beside it. */
    bool canClimb(int32_t x, int32_t feetY, int32_t z);

    /** A swimmer or flier can occupy this position. `swimming` selects water as the medium. */
    bool isOpen(int32_t x, int32_t feetY, int32_t z, bool swimming);

    bool isPassable(float centerX, float feetY, float centerZ);

    int32_t extraCost(int32_t x, int32_t feetY, int32_t z);

    bool hasBarrier(const Vector3f &from, const Vector3f &to);

private:
    enum BlockFlag : uint16_t {
        Collides = 1 << 0,
        Water = 1 << 1,
        Lava = 1 << 2,
        Damaging = 1 << 3,
        Fence = 1 << 4,
        ClosedDoor = 1 << 5,
        Exposed = 1 << 6,
        Door = 1 << 7,
        Portal = 1 << 8,
        Avoided = 1 << 9
    };

    struct CachedBlock {
        AxisAlignedBB mShape;
        uint16_t mFlags = 0;
    };

    static constexpr uint32_t CACHE_CAPACITY = 8192;

    const CachedBlock &_block(int32_t x, int32_t y, int32_t z);

    CachedBlock _classify(int32_t x, int32_t y, int32_t z) const;

    bool _boxCollides(const AxisAlignedBB &box);

    bool _isForbidden(uint16_t flags) const;

    bool _isSampleOpen(const Vector3f &position);

    Level *mLevel = nullptr;
    const PathOptions *mOptions = nullptr;
    float mRadius = 0.3f;
    float mHeight = 1.8f;
    float mStartY = 0.0f;
    bool mInWater = false;
    bool mAvoidSunNow = false;
    uint32_t mStamp = 0;

    std::array<int64_t, CACHE_CAPACITY> mKeys{};
    std::array<uint32_t, CACHE_CAPACITY> mStamps{};
    std::array<CachedBlock, CACHE_CAPACITY> mBlocks{};
};
