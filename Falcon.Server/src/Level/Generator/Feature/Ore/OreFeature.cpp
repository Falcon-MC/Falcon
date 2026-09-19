#include "Level/Generator/Feature/Ore/OreFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Core/Math/MathConstants.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Feature/FeatureMath.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_set>
#include <vector>

namespace {
    int64_t chunkHash(int32_t x, int32_t z) {
        return (((int64_t) x) << 32) | ((int64_t) (uint32_t) z);
    }

    int32_t randomRangeTriangle(IRandom &random, int32_t start, int32_t end) {
        const int32_t heightDiff = std::abs(end - start);
        const int32_t heightDiffHalf = heightDiff / 2;
        const int32_t heightDiffHalf2 = heightDiff - heightDiffHalf;
        const int32_t first = IFeature::randomRange(random, 0, heightDiffHalf2);
        const int32_t second = IFeature::randomRange(random, 0, heightDiffHalf);
        return std::min(start, end) + first + second;
    }

    struct OrePosition {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
    };

    class OrePlacementManager : public BlockManager {
    public:
        explicit OrePlacementManager(Level &level) : BlockManager(level) {
        }

        void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override {
            if (y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y) {
                const int64_t key = hashXYZ(x, y, z);
                if (mSeen.insert(key).second)
                    mPositions.push_back(OrePosition{x, y, z});
            }

            BlockManager::setBlockStateAt(x, y, z, state);
        }

        const std::vector<OrePosition> &getPositions() const {
            return mPositions;
        }

    private:
        std::unordered_set<int64_t> mSeen;
        std::vector<OrePosition> mPositions;
    };

    bool isAirAt(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        if (state == nullptr)
            return true;

        return state->mName == "minecraft:air";
    }
}

const BlockState &OreFeature::stoneState() {
    static const BlockState state = VanillaBlocks::STONE().toBlockState();
    return state;
}

const BlockState &OreFeature::deepslateState() {
    static const BlockState state = VanillaBlocks::DEEPSLATE().toBlockState();
    return state;
}

const BlockState &OreFeature::netherrackState() {
    static const BlockState state = VanillaBlocks::NETHERRACK().toBlockState();
    return state;
}

float OreFeature::getSkipAir() const {
    return 0.0f;
}

OreFeature::ConcentrationType OreFeature::getConcentration() const {
    return ConcentrationType::UNIFORM;
}

bool OreFeature::isRare() const {
    return false;
}

bool OreFeature::canBeReplaced(const BlockState &state) const {
    return state == stoneState() || state == deepslateState() || state == netherrackState();
}

void OreFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(chunkHash(chunkX, chunkZ) ^ (level.getSeed() + javaStringHash(name())));

    const int32_t startX = chunkX << 4;
    const int32_t startZ = chunkZ << 4;
    BlockManager manager(level);

    for (int32_t i = 0;
         i < (isRare() ? (mRandom.nextInt(getClusterCount()) == 0 ? 1 : 0) : getClusterCount());
         i++) {
        OrePlacementManager object(level);
        const int32_t maxY = std::min(getMaxHeight(), (int32_t) LevelChunk::MAX_Y);
        const int32_t minY = std::max(getMinHeight(), (int32_t) LevelChunk::MIN_Y);
        const int32_t x = startX + mRandom.nextInt(15);
        const int32_t z = startZ + mRandom.nextInt(15);

        int32_t y;
        if (getConcentration() == ConcentrationType::TRIANGLE)
            y = randomRangeTriangle(mRandom, minY, maxY);
        else
            y = minY + mRandom.nextBoundedInt((maxY - minY) + 1);

        const BlockState *originalPtr = level.peekBlockPtr(x, y, z);
        const BlockState original = originalPtr == nullptr ? BlockState() : *originalPtr;
        if (!canBeReplaced(original))
            continue;

        if (getClusterSize() == 1) {
            object.setBlockStateAt(x, y, z, getState(original));
        } else {
            mRandom.setSeed(level.getSeed() ^ chunkHash(chunkX, chunkZ) ^ (x + y + z));
            spawn(object, mRandom, x, y, z);
            mRandom.setSeed(level.getSeed() ^ chunkHash(chunkX, chunkZ));
        }

        bool skip = false;
        if (getSkipAir() != 0.0f) {
            bool air = false;
            for (const OrePosition &position: object.getPositions()) {
                if (isAirAt(level, position.mX, position.mY, position.mZ)) {
                    air = true;
                    break;
                }
            }

            if (air)
                skip = mRandom.nextFloat() < getSkipAir();
        }

        if (!skip) {
            for (const OrePosition &position: object.getPositions()) {
                if (level.peekChunkPtr(position.mX >> 4, position.mZ >> 4) == nullptr)
                    continue;

                manager.setBlockStateAt(position.mX, position.mY, position.mZ,
                                        object.getBlockAt(position.mX, position.mY, position.mZ));
            }
        }
    }

    queueObject(manager);
}

void OreFeature::spawn(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const int32_t clusterSize = getClusterSize();
    const float piScaled = random.nextFloat() * MathConstants::PI_F;

    const float maxXFloat = (float) (x + 8) + FeatureMath::sinLookup(piScaled) * (float) clusterSize / 8.0f;
    const float minXFloat = (float) (x + 8) - FeatureMath::sinLookup(piScaled) * (float) clusterSize / 8.0f;
    const float maxZFloat = (float) (z + 8) + FeatureMath::cosLookup(piScaled) * (float) clusterSize / 8.0f;
    const float minZFloat = (float) (z + 8) - FeatureMath::cosLookup(piScaled) * (float) clusterSize / 8.0f;

    const double scaleMaxX = (double) maxXFloat;
    const double scaleMinX = (double) minXFloat;
    const double scaleMaxZ = (double) maxZFloat;
    const double scaleMinZ = (double) minZFloat;
    const double scaleMaxY = (double) (y + random.nextBoundedInt(3) - 2);
    const double scaleMinY = (double) (y + random.nextBoundedInt(3) - 2);

    for (int32_t i = 0; i < clusterSize; ++i) {
        const float sizeIncr = (float) i / (float) clusterSize;
        const double scaleX = scaleMaxX + (scaleMinX - scaleMaxX) * (double) sizeIncr;
        const double scaleY = scaleMaxY + (scaleMinY - scaleMaxY) * (double) sizeIncr;
        const double scaleZ = scaleMaxZ + (scaleMinZ - scaleMaxZ) * (double) sizeIncr;
        const double randSizeOffset = random.nextDouble() * (double) clusterSize / 16.0;
        const double randVec1 = (double) (FeatureMath::sinLookup(MathConstants::PI_F * sizeIncr) + 1.0f)
                                * randSizeOffset + 1.0;
        const double randVec2 = (double) (FeatureMath::sinLookup(MathConstants::PI_F * sizeIncr) + 1.0f)
                                * randSizeOffset + 1.0;

        const int32_t minX = FeatureMath::floorDouble(scaleX - randVec1 / 2.0);
        const int32_t minY = FeatureMath::floorDouble(scaleY - randVec2 / 2.0);
        const int32_t minZ = FeatureMath::floorDouble(scaleZ - randVec1 / 2.0);
        const int32_t maxX = FeatureMath::floorDouble(scaleX + randVec1 / 2.0);
        const int32_t maxY = FeatureMath::floorDouble(scaleY + randVec2 / 2.0);
        const int32_t maxZ = FeatureMath::floorDouble(scaleZ + randVec1 / 2.0);

        for (int32_t xSeg = minX; xSeg <= maxX; ++xSeg) {
            const double xVal = ((double) xSeg + 0.5 - scaleX) / (randVec1 / 2.0);
            if (xVal * xVal >= 1.0)
                continue;

            for (int32_t ySeg = minY; ySeg <= maxY; ++ySeg) {
                if (ySeg < -64)
                    continue;

                const double yVal = ((double) ySeg + 0.5 - scaleY) / (randVec2 / 2.0);
                if (xVal * xVal + yVal * yVal >= 1.0)
                    continue;

                for (int32_t zSeg = minZ; zSeg <= maxZ; ++zSeg) {
                    const double zVal = ((double) zSeg + 0.5 - scaleZ) / (randVec1 / 2.0);
                    if (xVal * xVal + yVal * yVal + zVal * zVal >= 1.0)
                        continue;

                    const BlockState &original = manager.getBlockAt(xSeg, ySeg, zSeg);
                    if (canBeReplaced(original))
                        manager.setBlockStateAt(xSeg, ySeg, zSeg, getState(original));
                }
            }
        }
    }
}
