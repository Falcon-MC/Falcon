#include "Actor/AI/Navigation/WalkNodeEvaluator.h"

#include "Block/BlockShape.h"
#include "Block/Blocks/FenceBlock.h"
#include "Block/Blocks/FenceGateOrientationBlock.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"

#include <algorithm>
#include <cmath>

namespace {
    const float HALF_BLOCK = 0.5f;
    const float BOX_EPSILON = 1.0e-4f;
    const float BARRIER_SAMPLES_PER_BLOCK = 4.0f;

    int64_t packPosition(int32_t x, int32_t y, int32_t z) {
        return ((int64_t) (x & 0x3FFFFFF) << 38) | ((int64_t) (z & 0x3FFFFFF) << 12) | (int64_t) (y & 0xFFF);
    }

    uint32_t hashPosition(int64_t key) {
        uint64_t value = (uint64_t) key * 0x9E3779B97F4A7C15ull;
        return (uint32_t) (value >> 32);
    }
}

void WalkNodeEvaluator::prepare(Level &level, float width, float height, float startY, bool inWater) {
    mLevel = &level;
    mRadius = width * HALF_BLOCK;
    mHeight = height;
    mStartY = startY;
    mInWater = inWater;

    mStamp++;
    if (mStamp == 0) {
        mStamps.fill(0);
        mStamp = 1;
    }
}

int32_t WalkNodeEvaluator::availableOffset(int32_t x, int32_t feetY, int32_t z) {
    for (int32_t y = feetY; y >= feetY - MAX_DROP; --y) {
        if (isStandable(x, y, z))
            return y - feetY + 1;
    }
    return NO_OFFSET;
}

bool WalkNodeEvaluator::isStandable(int32_t x, int32_t y, int32_t z) {
    const uint8_t flags = _block(x, y, z).mFlags;
    if ((flags & (Lava | Cactus | Fence)) != 0)
        return false;

    if (mInWater && (float) (y + 1) - mStartY > 1.0f)
        return false;

    if ((flags & (Collides | Water)) == 0)
        return false;

    return isPassable((float) x + HALF_BLOCK, (float) (y + 1), (float) z + HALF_BLOCK);
}

bool WalkNodeEvaluator::isPassable(float centerX, float feetY, float centerZ) {
    const AxisAlignedBB box(centerX - mRadius, feetY, centerZ - mRadius, centerX + mRadius, feetY + mHeight,
                            centerZ + mRadius);
    if (!_boxCollides(box))
        return true;

    if (mRadius <= HALF_BLOCK)
        return false;

    const float shift = mRadius - HALF_BLOCK;
    for (int32_t i = -1; i <= 1; ++i) {
        for (int32_t j = -1; j <= 1; ++j) {
            if (i == 0 && j == 0)
                continue;

            if (!_boxCollides(box.offset((float) i * shift, 0.0f, (float) j * shift)))
                return true;
        }
    }
    return false;
}

int32_t WalkNodeEvaluator::extraCost(int32_t x, int32_t feetY, int32_t z) {
    int32_t cost = 0;
    if ((_block(x, feetY, z).mFlags & Water) != 0)
        cost += LIQUID_EXTRA_COST;
    if ((_block(x, feetY - 1, z).mFlags & Water) != 0)
        cost += LIQUID_EXTRA_COST;
    return cost;
}

bool WalkNodeEvaluator::hasBarrier(const Vector3f &from, const Vector3f &to) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    if (dx == 0.0f && dy == 0.0f && dz == 0.0f)
        return false;

    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const int32_t samples = std::max(1, (int32_t) std::ceil(distance * BARRIER_SAMPLES_PER_BLOCK));

    for (int32_t i = 0; i <= samples; ++i) {
        const float progress = (float) i / (float) samples;
        const float x = from.x + dx * progress;
        const float y = from.y + dy * progress;
        const float z = from.z + dz * progress;

        if (!isStandable((int32_t) std::floor(x), (int32_t) std::floor(y - 1.0f), (int32_t) std::floor(z)))
            return true;

        if (!isPassable(x, y, z))
            return true;
    }
    return false;
}

const WalkNodeEvaluator::CachedBlock &WalkNodeEvaluator::_block(int32_t x, int32_t y, int32_t z) {
    const int64_t key = packPosition(x, y, z);
    uint32_t slot = hashPosition(key) & (CACHE_CAPACITY - 1);

    for (uint32_t probe = 0; probe < CACHE_CAPACITY; ++probe) {
        if (mStamps[slot] != mStamp) {
            mStamps[slot] = mStamp;
            mKeys[slot] = key;
            mBlocks[slot] = _classify(x, y, z);
            return mBlocks[slot];
        }

        if (mKeys[slot] == key)
            return mBlocks[slot];

        slot = (slot + 1) & (CACHE_CAPACITY - 1);
    }

    mStamps[slot] = mStamp;
    mKeys[slot] = key;
    mBlocks[slot] = _classify(x, y, z);
    return mBlocks[slot];
}

WalkNodeEvaluator::CachedBlock WalkNodeEvaluator::_classify(int32_t x, int32_t y, int32_t z) const {
    CachedBlock block;
    const BlockState *state = mLevel->peekBlockPtr(x, y, z);
    if (state == nullptr)
        return block;

    const LiquidView liquid(*state);
    if (liquid.isLava())
        block.mFlags |= Lava;
    if (liquid.isWater())
        block.mFlags |= Water;

    const BlockState *overlay = mLevel->peekBlockPtr(x, y, z, 1);
    if (overlay != nullptr && LiquidView(*overlay).isWater())
        block.mFlags |= Water;

    if (state->mName == "minecraft:cactus")
        block.mFlags |= Cactus;

    const Block *definition = VanillaBlocks::fromIdentifier(state->mName);
    if (dynamic_cast<const FenceBlock *>(definition) != nullptr
        || dynamic_cast<const FenceGateOrientationBlock *>(definition) != nullptr)
        block.mFlags |= Fence;

    if (BlockShape::hasCollision(*state)) {
        block.mFlags |= Collides;
        block.mShape = BlockShape::getShapeAt(*state, x, y, z);
    }

    return block;
}

bool WalkNodeEvaluator::_boxCollides(const AxisAlignedBB &box) {
    const int32_t minX = (int32_t) std::floor(box.mMinX);
    const int32_t minY = (int32_t) std::floor(box.mMinY) - 1;
    const int32_t minZ = (int32_t) std::floor(box.mMinZ);
    const int32_t maxX = (int32_t) std::floor(box.mMaxX - BOX_EPSILON);
    const int32_t maxY = (int32_t) std::floor(box.mMaxY - BOX_EPSILON);
    const int32_t maxZ = (int32_t) std::floor(box.mMaxZ - BOX_EPSILON);

    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t z = minZ; z <= maxZ; ++z) {
            for (int32_t y = minY; y <= maxY; ++y) {
                const CachedBlock &block = _block(x, y, z);
                if ((block.mFlags & Collides) != 0 && block.mShape.intersectsWith(box))
                    return true;
            }
        }
    }
    return false;
}
