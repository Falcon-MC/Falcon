#pragma once

#include "Actor/AI/Navigation/PathOptions.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"

#include <array>
#include <cstdint>

class Level;

class WalkNodeEvaluator {
public:
    static constexpr int32_t NO_OFFSET = INT32_MIN;
    static constexpr int32_t MAX_DROP = 4;
    static constexpr int32_t LIQUID_EXTRA_COST = 20;

    void prepare(Level &level, float width, float height, const Vector3f &start, bool inWater,
                 const PathOptions &options);

    int32_t availableOffset(int32_t x, int32_t feetY, int32_t z);

    bool isStandable(int32_t x, int32_t y, int32_t z);

    bool isPassable(float centerX, float feetY, float centerZ);

    int32_t extraCost(int32_t x, int32_t feetY, int32_t z);

    bool hasBarrier(const Vector3f &from, const Vector3f &to);

    bool isInWater() const {
        return mInWater;
    }

private:
    enum BlockFlag : uint8_t {
        Collides = 1 << 0,
        Water = 1 << 1,
        Lava = 1 << 2,
        Cactus = 1 << 3,
        Fence = 1 << 4,
        ClosedDoor = 1 << 5,
        Exposed = 1 << 6
    };

    struct CachedBlock {
        AxisAlignedBB mShape;
        uint8_t mFlags = 0;
    };

    static constexpr uint32_t CACHE_CAPACITY = 8192;

    const CachedBlock &_block(int32_t x, int32_t y, int32_t z);

    CachedBlock _classify(int32_t x, int32_t y, int32_t z) const;

    bool _boxCollides(const AxisAlignedBB &box);

    Level *mLevel = nullptr;
    float mRadius = 0.3f;
    float mHeight = 1.8f;
    float mStartY = 0.0f;
    bool mInWater = false;
    bool mCanOpenDoors = false;
    bool mAvoidSun = false;
    uint32_t mStamp = 0;

    std::array<int64_t, CACHE_CAPACITY> mKeys{};
    std::array<uint32_t, CACHE_CAPACITY> mStamps{};
    std::array<CachedBlock, CACHE_CAPACITY> mBlocks{};
};
