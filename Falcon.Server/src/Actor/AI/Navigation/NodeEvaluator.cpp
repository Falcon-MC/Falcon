#include "Actor/AI/Navigation/NodeEvaluator.h"

#include "Block/BlockShape.h"
#include "Block/Blocks/DoorBlock.h"
#include "Block/Blocks/DoorOrientationBlock.h"
#include "Block/Blocks/FenceBlock.h"
#include "Block/Blocks/FenceGateBlock.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"

#include <algorithm>
#include <cmath>

namespace {
    const float HALF_BLOCK = 0.5f;
    const float BOX_EPSILON = 1.0e-4f;
    const float BARRIER_SAMPLES_PER_BLOCK = 4.0f;
    const int64_t DAYTIME_END = 12000;

    const int32_t ORTHOGONAL_X[] = {1, 0, -1, 0};
    const int32_t ORTHOGONAL_Z[] = {0, 1, 0, -1};

    int64_t packPosition(int32_t x, int32_t y, int32_t z) {
        return ((int64_t) (x & 0x3FFFFFF) << 38) | ((int64_t) (z & 0x3FFFFFF) << 12) | (int64_t) (y & 0xFFF);
    }

    uint32_t hashPosition(int64_t key) {
        uint64_t value = (uint64_t) key * 0x9E3779B97F4A7C15ull;
        return (uint32_t) (value >> 32);
    }

    bool isSunExposed(Level &level, int32_t x, int32_t y, int32_t z) {
        return y >= level.getHeightAt(x, z);
    }

    bool isSunny(const Level &level) {
        return level.hasSkyLight() && level.getDayTime() < DAYTIME_END && !level.isRaining();
    }
}

void NodeEvaluator::prepare(Level &level, float width, float height, const Vector3f &start, bool inWater,
                            const PathOptions &options) {
    mLevel = &level;
    mOptions = &options;
    mRadius = width * HALF_BLOCK;
    mHeight = height;
    mStartY = start.y;
    mInWater = inWater;
    mAvoidSunNow = options.mAvoidSun && isSunny(level)
                   && !isSunExposed(level, (int32_t) std::floor(start.x), (int32_t) std::floor(start.y),
                                    (int32_t) std::floor(start.z));

    mStamp++;
    if (mStamp == 0) {
        mStamps.fill(0);
        mStamp = 1;
    }
}

int32_t NodeEvaluator::availableOffset(int32_t x, int32_t feetY, int32_t z) {
    if (mOptions->mMode == NavigationMode::Climb && canClimb(x, feetY, z))
        return 0;

    for (int32_t y = feetY; y >= feetY - MAX_DROP; --y) {
        if (!isStandable(x, y, z))
            continue;

        const int32_t offset = y - feetY + 1;
        return offset > 0 && !mOptions->mCanJump ? NO_OFFSET : offset;
    }
    return NO_OFFSET;
}

bool NodeEvaluator::isStandable(int32_t x, int32_t y, int32_t z) {
    const uint16_t ground = _block(x, y, z).mFlags;
    const uint16_t feet = _block(x, y + 1, z).mFlags;
    if ((ground & Fence) != 0 || _isForbidden(ground) || _isForbidden(feet))
        return false;

    if ((ground & Lava) != 0 && !mOptions->mCanPathOverLava && !mOptions->mCanWalkInLava)
        return false;

    if ((ground & Water) != 0 && mOptions->mAvoidWater && !mInWater)
        return false;

    if (mInWater && (float) (y + 1) - mStartY > 1.0f)
        return false;

    if ((ground & (Collides | Water | Lava)) == 0)
        return false;

    if ((feet & (ClosedDoor | Exposed)) != 0)
        return false;

    return isPassable((float) x + HALF_BLOCK, (float) (y + 1), (float) z + HALF_BLOCK);
}

bool NodeEvaluator::canClimb(int32_t x, int32_t feetY, int32_t z) {
    if (_isForbidden(_block(x, feetY, z).mFlags))
        return false;

    bool wall = false;
    for (int32_t direction = 0; direction < 4 && !wall; ++direction)
        wall = (_block(x + ORTHOGONAL_X[direction], feetY, z + ORTHOGONAL_Z[direction]).mFlags & Collides) != 0;

    return wall && isPassable((float) x + HALF_BLOCK, (float) feetY, (float) z + HALF_BLOCK);
}

bool NodeEvaluator::isOpen(int32_t x, int32_t feetY, int32_t z, bool swimming) {
    const uint16_t feet = _block(x, feetY, z).mFlags;
    if ((feet & Lava) != 0 || _isForbidden(feet))
        return false;

    if (swimming) {
        const bool breaching = mOptions->mCanBreach && (_block(x, feetY - 1, z).mFlags & Water) != 0;
        if ((feet & Water) == 0 && !breaching)
            return false;
    } else if ((feet & Water) != 0 && !mOptions->swimsThroughWater()) {
        return false;
    }

    return isPassable((float) x + HALF_BLOCK, (float) feetY, (float) z + HALF_BLOCK);
}

bool NodeEvaluator::isPassable(float centerX, float feetY, float centerZ) {
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

int32_t NodeEvaluator::extraCost(int32_t x, int32_t feetY, int32_t z) {
    const uint16_t feet = _block(x, feetY, z).mFlags;
    const uint16_t ground = _block(x, feetY - 1, z).mFlags;
    int32_t cost = 0;

    const bool wades = !mOptions->isVolumetric() && !mOptions->mCanPathOverWater && !mOptions->swimsThroughWater();
    if (wades && (feet & Water) != 0)
        cost += LIQUID_EXTRA_COST;
    if (wades && (ground & Water) != 0)
        cost += LIQUID_EXTRA_COST;

    if (((feet | ground) & Damaging) != 0)
        cost += HAZARD_EXTRA_COST;
    return cost;
}

bool NodeEvaluator::hasBarrier(const Vector3f &from, const Vector3f &to) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    if (dx == 0.0f && dy == 0.0f && dz == 0.0f)
        return false;

    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const int32_t samples = std::max(1, (int32_t) std::ceil(distance * BARRIER_SAMPLES_PER_BLOCK));

    for (int32_t i = 0; i <= samples; ++i) {
        const float progress = (float) i / (float) samples;
        if (!_isSampleOpen(Vector3f(from.x + dx * progress, from.y + dy * progress, from.z + dz * progress)))
            return true;
    }
    return false;
}

bool NodeEvaluator::_isSampleOpen(const Vector3f &position) {
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);

    if (mOptions->isVolumetric())
        return isOpen(x, y, z, mOptions->mMode == NavigationMode::Swim) && isPassable(position.x, position.y,
                                                                                         position.z);

    const bool supported = isStandable(x, (int32_t) std::floor(position.y - 1.0f), z)
                           || (mOptions->mMode == NavigationMode::Climb && canClimb(x, y, z))
                           || (mOptions->swimsThroughWater() && isOpen(x, y, z, true));
    return supported && isPassable(position.x, position.y, position.z);
}

bool NodeEvaluator::_isForbidden(uint16_t flags) const {
    if ((flags & Avoided) != 0)
        return true;
    if ((flags & Damaging) != 0 && mOptions->mAvoidDamageBlocks)
        return true;
    if ((flags & Portal) != 0 && mOptions->mAvoidPortals)
        return true;
    return (flags & Door) != 0 && !mOptions->mCanPassDoors;
}

const NodeEvaluator::CachedBlock &NodeEvaluator::_block(int32_t x, int32_t y, int32_t z) {
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

NodeEvaluator::CachedBlock NodeEvaluator::_classify(int32_t x, int32_t y, int32_t z) const {
    CachedBlock block;
    const BlockState *state = mLevel->peekBlockPtr(x, y, z);
    if (state == nullptr)
        return block;

    if (mAvoidSunNow && isSunExposed(*mLevel, x, y, z))
        block.mFlags |= Exposed;

    const LiquidView liquid(*state);
    if (liquid.isLava())
        block.mFlags |= Lava;
    if (liquid.isWater())
        block.mFlags |= Water;

    const BlockState *overlay = mLevel->peekBlockPtr(x, y, z, 1);
    if (overlay != nullptr && LiquidView(*overlay).isWater())
        block.mFlags |= Water;

    if (std::find(mOptions->mBlocksToAvoid.begin(), mOptions->mBlocksToAvoid.end(), state->mName)
        != mOptions->mBlocksToAvoid.end())
        block.mFlags |= Avoided;

    const Block *definition = VanillaBlocks::fromIdentifier(state->mName);
    const PathHazard hazard = definition == nullptr ? PathHazard::None : definition->getPathHazard();
    if (hazard == PathHazard::Damaging)
        block.mFlags |= Damaging;
    if (hazard == PathHazard::Portal)
        block.mFlags |= Portal;

    if (dynamic_cast<const FenceBlock *>(definition) != nullptr
        || dynamic_cast<const FenceGateBlock *>(definition) != nullptr)
        block.mFlags |= Fence;

    const bool door = dynamic_cast<const DoorOrientationBlock *>(definition) != nullptr;
    if (door)
        block.mFlags |= Door;

    const bool closedDoor = door && !OpenableBlock::isOpen(*state);
    const bool wooden = DoorBlock::isOpenableByHand(state->mName);
    const bool opensDoor = closedDoor && wooden && (mOptions->mCanOpenDoors || mOptions->mCanBreakDoors);
    if (closedDoor && !opensDoor)
        block.mFlags |= ClosedDoor;

    if (!opensDoor && BlockShape::hasCollision(*state)) {
        block.mFlags |= Collides;
        block.mShape = BlockShape::getShapeAt(*state, x, y, z);
    }

    return block;
}

bool NodeEvaluator::_boxCollides(const AxisAlignedBB &box) {
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
