#include "World/Level/LevelGen/Material/Aquifer.h"

#include "World/Level/Block/VanillaBlocks.h"

#include <cmath>
#include <limits>
#include <utility>

namespace {

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &stoneState() {
        static const BlockState state = VanillaBlocks::STONE().toBlockState();
        return state;
    }

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    int32_t javaFloorDiv(int32_t x, int32_t y) {
        int32_t quotient = x / y;
        if ((x % y != 0) && ((x ^ y) < 0))
            quotient--;

        return quotient;
    }

    double javaClamp(double value, double min, double max) {
        return value < min ? min : (value < max ? value : max);
    }

}

const int32_t Aquifer::SURFACE_SAMPLING_OFFSETS_IN_CHUNKS[Aquifer::SURFACE_SAMPLING_OFFSET_COUNT][2] = {
        {0,  0},
        {-2, -1},
        {-1, -1},
        {0,  -1},
        {1,  -1},
        {-3, 0},
        {-2, 0},
        {-1, 0},
        {1,  0},
        {-2, 1},
        {-1, 1},
        {0,  1},
        {1,  1}
};

const double Aquifer::FLOWING_UPDATE_SIMILARITY = Aquifer::_similarity(100, 144);

Aquifer::FluidStatus::FluidStatus() : mFluidLevel(0), mFluidType(&airState()) {
}

Aquifer::FluidStatus::FluidStatus(int32_t fluidLevel, const BlockState *fluidType)
        : mFluidLevel(fluidLevel), mFluidType(fluidType) {
}

const BlockState *Aquifer::FluidStatus::at(int32_t blockY) const {
    return blockY < mFluidLevel ? mFluidType : &airState();
}

bool Aquifer::FluidStatus::operator==(const FluidStatus &other) const {
    return mFluidLevel == other.mFluidLevel && *mFluidType == *other.mFluidType;
}

bool Aquifer::FluidStatus::operator!=(const FluidStatus &other) const {
    return !(*this == other);
}

Aquifer::FluidPicker Aquifer::overworldFluidPicker(int32_t seaLevel) {
    const BlockState *water = &waterState();
    const BlockState *lava = &lavaState();
    const int32_t lavaLevel = -54;
    const int32_t lavaThreshold = lavaLevel < seaLevel ? lavaLevel : seaLevel;
    return [water, lava, lavaLevel, lavaThreshold, seaLevel](int32_t x, int32_t y, int32_t z) -> FluidStatus {
        (void) x;
        (void) z;
        return y < lavaThreshold ? FluidStatus(lavaLevel, lava) : FluidStatus(seaLevel, water);
    };
}

Aquifer::Aquifer(int32_t chunkX, int32_t chunkZ, int64_t levelSeed, DensityChunkCache *chunkCache,
                 DensityFunctionPtr barrierNoise, DensityFunctionPtr fluidLevelFloodednessNoise,
                 DensityFunctionPtr fluidLevelSpreadNoise, DensityFunctionPtr lavaNoise,
                 DensityFunctionPtr erosion, DensityFunctionPtr depth,
                 DensityFunctionPtr preliminarySurfaceDensity, DensityFunctionPtr preliminarySurfaceUpperBound,
                 int32_t minAquiferY, int32_t aquiferCellHeight, int32_t minY, int32_t yBlockSize,
                 FluidPicker globalFluidPicker)
        : mBarrierNoise(std::move(barrierNoise)),
          mFluidLevelFloodednessNoise(std::move(fluidLevelFloodednessNoise)),
          mFluidLevelSpreadNoise(std::move(fluidLevelSpreadNoise)),
          mLavaNoise(std::move(lavaNoise)),
          mErosion(std::move(erosion)),
          mDepth(std::move(depth)),
          mPreliminarySurfaceDensity(std::move(preliminarySurfaceDensity)),
          mPreliminarySurfaceUpperBound(std::move(preliminarySurfaceUpperBound)),
          mGlobalFluidPicker(std::move(globalFluidPicker)),
          mSkipSamplingAboveY(0),
          mMinY(minY),
          mMaxY(minY + yBlockSize - 1),
          mMinGridX(0),
          mMinGridY(0),
          mMinGridZ(0),
          mGridSizeX(0),
          mGridSizeZ(0),
          mRandomSeed(levelSeed ^ (int64_t) 0x4f9939f508LL),
          mPreliminarySurfaceLowerBound(minAquiferY),
          mPreliminarySurfaceCellHeight(aquiferCellHeight),
          mCachedBarrierNoise(0.0),
          mShouldScheduleFluidUpdate(false),
          mRandom(0) {
    mCachedPointContext.withCache(chunkCache);

    const int32_t minBlockX = chunkX << 4;
    const int32_t maxBlockX = minBlockX + 15;
    const int32_t minBlockZ = chunkZ << 4;
    const int32_t maxBlockZ = minBlockZ + 15;

    mMinGridX = _gridX(minBlockX - 5);
    const int32_t maxGridX = _gridX(maxBlockX - 5) + 1;
    mGridSizeX = maxGridX - mMinGridX + 1;
    mMinGridY = _gridY(minY + 1) - 1;
    const int32_t maxGridY = _gridY(minY + yBlockSize + 1) + 1;
    const int32_t gridSizeY = maxGridY - mMinGridY + 1;
    mMinGridZ = _gridZ(minBlockZ - 5);
    const int32_t maxGridZ = _gridZ(maxBlockZ - 5) + 1;
    mGridSizeZ = maxGridZ - mMinGridZ + 1;

    const int32_t totalGridSize = mGridSizeX * gridSizeY * mGridSizeZ;
    mAquiferCache.assign((size_t) totalGridSize, std::optional<FluidStatus>());
    mAquiferLocationCache.assign((size_t) totalGridSize, 0);
    mAquiferOffsetCache.assign((size_t) totalGridSize, 0);
    _preloadAquiferLocations(gridSizeY);

    const int32_t maxAdjustedSurfaceLevel = _adjustSurfaceLevel(
            _maxPreliminarySurfaceLevel(
                    _fromGridX(mMinGridX, 0),
                    _fromGridZ(mMinGridZ, 0),
                    _fromGridX(maxGridX, 9),
                    _fromGridZ(maxGridZ, 9)
            )
    );
    const int32_t skipSamplingAboveGridY = _gridY(maxAdjustedSurfaceLevel + 12) + 1;
    mSkipSamplingAboveY = _fromGridY(skipSamplingAboveGridY, 11) - 1;
}

const BlockState *Aquifer::computeSubstance(FunctionContext &context, double density) {
    if (density > 0.0) {
        mShouldScheduleFluidUpdate = false;
        return nullptr;
    }

    const int32_t posX = context.blockX();
    const int32_t posY = context.blockY();
    const int32_t posZ = context.blockZ();
    const FluidStatus globalFluid = mGlobalFluidPicker(posX, posY, posZ);
    const BlockState *globalAtPos = globalFluid.at(posY);

    if (posY > mSkipSamplingAboveY) {
        mShouldScheduleFluidUpdate = false;
        return globalAtPos;
    }

    if (_isLava(*globalAtPos)) {
        mShouldScheduleFluidUpdate = false;
        return &lavaState();
    }

    const int32_t xAnchor = _gridX(posX - 5);
    const int32_t yAnchor = _gridY(posY + 1);
    const int32_t zAnchor = _gridZ(posZ - 5);

    int32_t distanceSqr1 = std::numeric_limits<int32_t>::max();
    int32_t distanceSqr2 = std::numeric_limits<int32_t>::max();
    int32_t distanceSqr3 = std::numeric_limits<int32_t>::max();
    int32_t distanceSqr4 = std::numeric_limits<int32_t>::max();
    int32_t closestIndex1 = 0;
    int32_t closestIndex2 = 0;
    int32_t closestIndex3 = 0;
    int32_t closestIndex4 = 0;

    for (int32_t x1 = 0; x1 <= 1; x1++) {
        for (int32_t y1 = -1; y1 <= 1; y1++) {
            for (int32_t z1 = 0; z1 <= 1; z1++) {
                const int32_t spacedGridX = xAnchor + x1;
                const int32_t spacedGridY = yAnchor + y1;
                const int32_t spacedGridZ = zAnchor + z1;
                const int32_t index = _getIndex(spacedGridX, spacedGridY, spacedGridZ);
                const int32_t packedOffset = mAquiferOffsetCache[(size_t) index];
                const int32_t dx = _fromGridX(spacedGridX, _unpackOffsetX(packedOffset)) - posX;
                const int32_t dy = _fromGridY(spacedGridY, _unpackOffsetY(packedOffset)) - posY;
                const int32_t dz = _fromGridZ(spacedGridZ, _unpackOffsetZ(packedOffset)) - posZ;
                const int32_t newDistance = dx * dx + dy * dy + dz * dz;
                if (distanceSqr1 >= newDistance) {
                    closestIndex4 = closestIndex3;
                    closestIndex3 = closestIndex2;
                    closestIndex2 = closestIndex1;
                    closestIndex1 = index;
                    distanceSqr4 = distanceSqr3;
                    distanceSqr3 = distanceSqr2;
                    distanceSqr2 = distanceSqr1;
                    distanceSqr1 = newDistance;
                } else if (distanceSqr2 >= newDistance) {
                    closestIndex4 = closestIndex3;
                    closestIndex3 = closestIndex2;
                    closestIndex2 = index;
                    distanceSqr4 = distanceSqr3;
                    distanceSqr3 = distanceSqr2;
                    distanceSqr2 = newDistance;
                } else if (distanceSqr3 >= newDistance) {
                    closestIndex4 = closestIndex3;
                    closestIndex3 = index;
                    distanceSqr4 = distanceSqr3;
                    distanceSqr3 = newDistance;
                } else if (distanceSqr4 >= newDistance) {
                    closestIndex4 = index;
                    distanceSqr4 = newDistance;
                }
            }
        }
    }

    const FluidStatus &closestStatus1 = _getAquiferStatus(closestIndex1);
    const double similarity12 = _similarity(distanceSqr1, distanceSqr2);
    const BlockState *fluidState = closestStatus1.at(posY);
    if (similarity12 <= 0.0) {
        if (similarity12 >= FLOWING_UPDATE_SIMILARITY) {
            const FluidStatus &closestStatus2 = _getAquiferStatus(closestIndex2);
            mShouldScheduleFluidUpdate = !(closestStatus1 == closestStatus2);
        } else {
            mShouldScheduleFluidUpdate = false;
        }

        return fluidState;
    }

    if (_isWater(*fluidState)) {
        const FluidStatus below = mGlobalFluidPicker(posX, posY - 1, posZ);
        if (_isLava(*below.at(posY - 1))) {
            mShouldScheduleFluidUpdate = true;
            return fluidState;
        }
    }

    mCachedBarrierNoise = std::numeric_limits<double>::quiet_NaN();
    const FluidStatus &closestStatus2 = _getAquiferStatus(closestIndex2);
    const double barrier12 = similarity12 * _calculatePressure(context, closestStatus1, closestStatus2);
    if (density + barrier12 > 0.0) {
        mShouldScheduleFluidUpdate = false;
        return &stoneState();
    }

    const FluidStatus &closestStatus3 = _getAquiferStatus(closestIndex3);
    const double similarity13 = _similarity(distanceSqr1, distanceSqr3);
    if (similarity13 > 0.0) {
        const double barrier13 = similarity12 * similarity13 * _calculatePressure(context, closestStatus1, closestStatus3);
        if (density + barrier13 > 0.0) {
            mShouldScheduleFluidUpdate = false;
            return &stoneState();
        }
    }

    const double similarity23 = _similarity(distanceSqr2, distanceSqr3);
    if (similarity23 > 0.0) {
        const double barrier23 = similarity12 * similarity23 * _calculatePressure(context, closestStatus2, closestStatus3);
        if (density + barrier23 > 0.0) {
            mShouldScheduleFluidUpdate = false;
            return &stoneState();
        }
    }

    const bool mayFlow12 = !(closestStatus1 == closestStatus2);
    const bool mayFlow23 = similarity23 >= FLOWING_UPDATE_SIMILARITY && !(closestStatus2 == closestStatus3);
    const bool mayFlow13 = similarity13 >= FLOWING_UPDATE_SIMILARITY && !(closestStatus1 == closestStatus3);
    if (!mayFlow12 && !mayFlow23 && !mayFlow13) {
        mShouldScheduleFluidUpdate = similarity13 >= FLOWING_UPDATE_SIMILARITY
                                     && _similarity(distanceSqr1, distanceSqr4) >= FLOWING_UPDATE_SIMILARITY
                                     && !(closestStatus1 == _getAquiferStatus(closestIndex4));
    } else {
        mShouldScheduleFluidUpdate = true;
    }

    return fluidState;
}

bool Aquifer::shouldScheduleFluidUpdate() const {
    return mShouldScheduleFluidUpdate;
}

int32_t Aquifer::_getIndex(int32_t gridX, int32_t gridY, int32_t gridZ) const {
    const int32_t x = gridX - mMinGridX;
    const int32_t y = gridY - mMinGridY;
    const int32_t z = gridZ - mMinGridZ;
    return (y * mGridSizeZ + z) * mGridSizeX + x;
}

void Aquifer::_preloadAquiferLocations(int32_t gridSizeY) {
    for (int32_t y = 0; y < gridSizeY; y++) {
        const int32_t gridY = mMinGridY + y;
        for (int32_t z = 0; z < mGridSizeZ; z++) {
            const int32_t gridZ = mMinGridZ + z;
            for (int32_t x = 0; x < mGridSizeX; x++) {
                const int32_t gridX = mMinGridX + x;
                mRandom.setSeed(_mixSeed(mRandomSeed, gridX, gridY, gridZ));
                const int32_t offsetX = mRandom.nextInt(X_RANGE);
                const int32_t offsetY = mRandom.nextInt(Y_RANGE);
                const int32_t offsetZ = mRandom.nextInt(Z_RANGE);
                const int32_t index = (y * mGridSizeZ + z) * mGridSizeX + x;
                mAquiferOffsetCache[(size_t) index] = _packOffset(offsetX, offsetY, offsetZ);
                mAquiferLocationCache[(size_t) index] = _pack(
                        _fromGridX(gridX, offsetX),
                        _fromGridY(gridY, offsetY),
                        _fromGridZ(gridZ, offsetZ)
                );
            }
        }
    }
}

double Aquifer::_calculatePressure(FunctionContext &context, const FluidStatus &statusClosest1,
                                   const FluidStatus &statusClosest2) {
    const int32_t posY = context.blockY();
    const BlockState *type1 = statusClosest1.at(posY);
    const BlockState *type2 = statusClosest2.at(posY);
    if ((!_isLava(*type1) || !_isWater(*type2)) && (!_isWater(*type1) || !_isLava(*type2))) {
        const int32_t fluidYDiff = std::abs(statusClosest1.mFluidLevel - statusClosest2.mFluidLevel);
        if (fluidYDiff == 0)
            return 0.0;

        const double averageFluidY = 0.5 * ((double) statusClosest1.mFluidLevel + (double) statusClosest2.mFluidLevel);
        const double howFarAboveAverageFluidPoint = (double) posY + 0.5 - averageFluidY;
        const double baseValue = fluidYDiff / 2.0;
        const double topBias = 0.0;
        const double furthestRocksFromTopBias = 2.5;
        const double furthestHolesFromTopBias = 1.5;
        const double bottomBias = 3.0;
        const double furthestRocksFromBottomBias = 10.0;
        const double furthestHolesFromBottomBias = 3.0;
        const double distanceFromBarrierEdgeTowardsMiddle = baseValue - std::fabs(howFarAboveAverageFluidPoint);
        double gradient;
        if (howFarAboveAverageFluidPoint > 0.0) {
            const double centerPoint = topBias + distanceFromBarrierEdgeTowardsMiddle;
            if (centerPoint > 0.0)
                gradient = centerPoint / furthestHolesFromTopBias;
            else
                gradient = centerPoint / furthestRocksFromTopBias;
        } else {
            const double centerPoint = bottomBias + distanceFromBarrierEdgeTowardsMiddle;
            if (centerPoint > 0.0)
                gradient = centerPoint / furthestHolesFromBottomBias;
            else
                gradient = centerPoint / furthestRocksFromBottomBias;
        }

        const double amplitude = 2.0;
        double noiseValue;
        if (gradient >= -amplitude && gradient <= amplitude) {
            const double currentNoiseValue = mCachedBarrierNoise;
            if (std::isnan(currentNoiseValue)) {
                const double barrierNoise = mBarrierNoise->compute(context);
                mCachedBarrierNoise = barrierNoise;
                noiseValue = barrierNoise;
            } else {
                noiseValue = currentNoiseValue;
            }
        } else {
            noiseValue = 0.0;
        }

        return amplitude * (noiseValue + gradient);
    }

    return 2.0;
}

const Aquifer::FluidStatus &Aquifer::_getAquiferStatus(int32_t index) {
    std::optional<FluidStatus> &slot = mAquiferCache[(size_t) index];
    if (slot.has_value())
        return slot.value();

    const int64_t location = mAquiferLocationCache[(size_t) index];
    slot = _computeFluid(_unpackX(location), _unpackY(location), _unpackZ(location));
    return slot.value();
}

Aquifer::FluidStatus Aquifer::_computeFluid(int32_t x, int32_t y, int32_t z) {
    const FluidStatus globalFluid = mGlobalFluidPicker(x, y, z);
    int32_t lowestPreliminarySurface = std::numeric_limits<int32_t>::max();
    const int32_t topOfAquiferCell = y + Y_SPACING;
    const int32_t bottomOfAquiferCell = y - Y_SPACING;
    bool surfaceAtCenterIsUnderGlobalFluidLevel = false;

    for (int32_t i = 0; i < SURFACE_SAMPLING_OFFSET_COUNT; i++) {
        const int32_t offsetX = SURFACE_SAMPLING_OFFSETS_IN_CHUNKS[i][0];
        const int32_t offsetZ = SURFACE_SAMPLING_OFFSETS_IN_CHUNKS[i][1];
        const int32_t sampleX = x + (offsetX << 4);
        const int32_t sampleZ = z + (offsetZ << 4);
        const int32_t preliminarySurfaceLevel = _preliminarySurfaceLevel(sampleX, sampleZ);
        const int32_t adjustedSurfaceLevel = _adjustSurfaceLevel(preliminarySurfaceLevel);
        const bool start = offsetX == 0 && offsetZ == 0;
        if (start && bottomOfAquiferCell > adjustedSurfaceLevel)
            return globalFluid;

        const bool topOfAquiferCellPokesAboveSurface = topOfAquiferCell > adjustedSurfaceLevel;
        if (topOfAquiferCellPokesAboveSurface || start) {
            const FluidStatus globalFluidAtSurface = mGlobalFluidPicker(sampleX, adjustedSurfaceLevel, sampleZ);
            if (!_isAir(*globalFluidAtSurface.at(adjustedSurfaceLevel))) {
                if (start)
                    surfaceAtCenterIsUnderGlobalFluidLevel = true;

                if (topOfAquiferCellPokesAboveSurface)
                    return globalFluidAtSurface;
            }
        }

        if (preliminarySurfaceLevel < lowestPreliminarySurface)
            lowestPreliminarySurface = preliminarySurfaceLevel;
    }

    const int32_t fluidSurfaceLevel = _computeSurfaceLevel(x, y, z, globalFluid, lowestPreliminarySurface,
                                                           surfaceAtCenterIsUnderGlobalFluidLevel);
    return FluidStatus(fluidSurfaceLevel, _computeFluidType(x, y, z, globalFluid, fluidSurfaceLevel));
}

int32_t Aquifer::_adjustSurfaceLevel(int32_t preliminarySurfaceLevel) {
    return preliminarySurfaceLevel + SURFACE_LEVEL_Y_OFFSET;
}

int32_t Aquifer::_computeSurfaceLevel(int32_t x, int32_t y, int32_t z, const FluidStatus &globalFluid,
                                      int32_t lowestPreliminarySurface, bool surfaceAtCenterIsUnderGlobalFluidLevel) {
    MutableChunkCacheContext &context = mCachedPointContext.set(x, y, z);
    double partiallyFloodedness;
    double fullyFloodedness;
    if (_isDeepDarkRegion(mErosion, mDepth, context)) {
        partiallyFloodedness = -1.0;
        fullyFloodedness = -1.0;
    } else {
        const int32_t distanceBelowSurface = lowestPreliminarySurface + SURFACE_LEVEL_Y_OFFSET - y;
        const double floodednessFactor = surfaceAtCenterIsUnderGlobalFluidLevel
                                         ? _clampedMap((double) distanceBelowSurface, 0.0, 64.0, 1.0, 0.0)
                                         : 0.0;
        const double floodednessNoiseValue = javaClamp(mFluidLevelFloodednessNoise->compute(context), -1.0, 1.0);
        const double fullyFloodedThreshold = _map(floodednessFactor, 1.0, 0.0, -0.3, 0.8);
        const double partiallyFloodedThreshold = _map(floodednessFactor, 1.0, 0.0, -0.8, 0.4);
        partiallyFloodedness = floodednessNoiseValue - partiallyFloodedThreshold;
        fullyFloodedness = floodednessNoiseValue - fullyFloodedThreshold;
    }

    if (fullyFloodedness > 0.0)
        return globalFluid.mFluidLevel;

    if (partiallyFloodedness > 0.0)
        return _computeRandomizedFluidSurfaceLevel(x, y, z, lowestPreliminarySurface);

    return WAY_BELOW_MIN_Y;
}

int32_t Aquifer::_computeRandomizedFluidSurfaceLevel(int32_t x, int32_t y, int32_t z,
                                                     int32_t lowestPreliminarySurface) {
    const int32_t fluidLevelCellX = javaFloorDiv(x, 16);
    const int32_t fluidLevelCellY = javaFloorDiv(y, 40);
    const int32_t fluidLevelCellZ = javaFloorDiv(z, 16);
    const int32_t fluidCellMiddleY = fluidLevelCellY * 40 + 20;
    const double fluidLevelSpread =
            mFluidLevelSpreadNoise->compute(mCachedPointContext.set(fluidLevelCellX, fluidLevelCellY, fluidLevelCellZ))
            * 10.0;
    const int32_t fluidLevelSpreadQuantized = _quantize(fluidLevelSpread, 3);
    const int32_t targetFluidSurfaceLevel = fluidCellMiddleY + fluidLevelSpreadQuantized;
    return lowestPreliminarySurface < targetFluidSurfaceLevel ? lowestPreliminarySurface : targetFluidSurfaceLevel;
}

const BlockState *Aquifer::_computeFluidType(int32_t x, int32_t y, int32_t z, const FluidStatus &globalFluid,
                                             int32_t fluidSurfaceLevel) {
    const BlockState *fluidType = globalFluid.mFluidType;
    if (fluidSurfaceLevel <= -10 && fluidSurfaceLevel != WAY_BELOW_MIN_Y && !_isLava(*globalFluid.mFluidType)) {
        const int32_t fluidTypeCellX = javaFloorDiv(x, 64);
        const int32_t fluidTypeCellY = javaFloorDiv(y, 40);
        const int32_t fluidTypeCellZ = javaFloorDiv(z, 64);
        const double lavaNoiseValue =
                mLavaNoise->compute(mCachedPointContext.set(fluidTypeCellX, fluidTypeCellY, fluidTypeCellZ));
        if (std::fabs(lavaNoiseValue) > 0.3)
            fluidType = &lavaState();
    }

    return fluidType;
}

int32_t Aquifer::_preliminarySurfaceLevel(int32_t worldX, int32_t worldZ) {
    const int64_t key = ((int64_t) worldX << 32) ^ ((int64_t) (uint32_t) worldZ);
    const auto found = mPreliminarySurfaceLevelCache.find(key);
    if (found != mPreliminarySurfaceLevelCache.end())
        return found->second;

    const int32_t lowerY = mMinY > mPreliminarySurfaceLowerBound ? mMinY : mPreliminarySurfaceLowerBound;
    int32_t upperY = (int32_t) std::floor(
            mPreliminarySurfaceUpperBound->compute(mCachedPointContext.set(worldX, 0, worldZ)));
    upperY = javaFloorDiv(upperY, mPreliminarySurfaceCellHeight) * mPreliminarySurfaceCellHeight;
    if (mMaxY < upperY)
        upperY = mMaxY;

    int32_t result = lowerY;
    if (upperY > lowerY) {
        for (int32_t y = upperY; y >= lowerY; y -= mPreliminarySurfaceCellHeight) {
            if (mPreliminarySurfaceDensity->compute(mCachedPointContext.set(worldX, y, worldZ)) > 0.0) {
                result = y;
                break;
            }
        }
    }

    mPreliminarySurfaceLevelCache[key] = result;
    return result;
}

int32_t Aquifer::_maxPreliminarySurfaceLevel(int32_t minX, int32_t minZ, int32_t maxX, int32_t maxZ) {
    int32_t maxSurface = std::numeric_limits<int32_t>::min();
    for (int32_t x = minX; x <= maxX; x += 4) {
        for (int32_t z = minZ; z <= maxZ; z += 4) {
            const int32_t level = _preliminarySurfaceLevel(x, z);
            if (level > maxSurface)
                maxSurface = level;
        }
    }

    return maxSurface == std::numeric_limits<int32_t>::min() ? 63 : maxSurface;
}

int32_t Aquifer::_gridX(int32_t blockCoord) {
    return blockCoord >> 4;
}

int32_t Aquifer::_fromGridX(int32_t gridCoord, int32_t blockOffset) {
    return (gridCoord << 4) + blockOffset;
}

int32_t Aquifer::_gridY(int32_t blockCoord) {
    return javaFloorDiv(blockCoord, 12);
}

int32_t Aquifer::_fromGridY(int32_t gridCoord, int32_t blockOffset) {
    return gridCoord * 12 + blockOffset;
}

int32_t Aquifer::_gridZ(int32_t blockCoord) {
    return blockCoord >> 4;
}

int32_t Aquifer::_fromGridZ(int32_t gridCoord, int32_t blockOffset) {
    return (gridCoord << 4) + blockOffset;
}

int64_t Aquifer::_pack(int32_t x, int32_t y, int32_t z) {
    const uint64_t px = (uint64_t) (uint32_t) (x & 0x3FFFFFF) << 38;
    const uint64_t pz = (uint64_t) (uint32_t) (z & 0x3FFFFFF) << 12;
    const uint64_t py = (uint64_t) (uint32_t) (y & 0xFFF);
    return (int64_t) (px | pz | py);
}

int16_t Aquifer::_packOffset(int32_t x, int32_t y, int32_t z) {
    return (int16_t) ((x << 8) | (y << 4) | z);
}

int32_t Aquifer::_unpackOffsetX(int32_t packed) {
    return (packed >> 8) & 0xF;
}

int32_t Aquifer::_unpackOffsetY(int32_t packed) {
    return (packed >> 4) & 0xF;
}

int32_t Aquifer::_unpackOffsetZ(int32_t packed) {
    return packed & 0xF;
}

int32_t Aquifer::_unpackX(int64_t packed) {
    return (int32_t) (packed >> 38);
}

int32_t Aquifer::_unpackY(int64_t packed) {
    const int64_t shifted = (int64_t) ((uint64_t) packed << 52);
    return (int32_t) (shifted >> 52);
}

int32_t Aquifer::_unpackZ(int64_t packed) {
    const int64_t shifted = (int64_t) ((uint64_t) packed << 26);
    return (int32_t) (shifted >> 38);
}

bool Aquifer::_isDeepDarkRegion(const DensityFunctionPtr &erosion, const DensityFunctionPtr &depth,
                                FunctionContext &context) {
    return erosion->compute(context) < -0.225 && depth->compute(context) > 0.9;
}

double Aquifer::_similarity(int32_t distanceSqr1, int32_t distanceSqr2) {
    return 1.0 - (distanceSqr2 - distanceSqr1) / 25.0;
}

double Aquifer::_clampedMap(double value, double inMin, double inMax, double outMin, double outMax) {
    if (inMin == inMax)
        return value < inMin ? outMin : outMax;

    const double t = javaClamp((value - inMin) / (inMax - inMin), 0.0, 1.0);
    return outMin + (outMax - outMin) * t;
}

double Aquifer::_map(double value, double inMin, double inMax, double outMin, double outMax) {
    if (inMin == inMax)
        return outMin;

    const double t = (value - inMin) / (inMax - inMin);
    return outMin + (outMax - outMin) * t;
}

int32_t Aquifer::_quantize(double value, int32_t step) {
    return (int32_t) std::floor(value / step) * step;
}

int64_t Aquifer::_mixSeed(int64_t seed, int32_t x, int32_t y, int32_t z) {
    uint64_t mixed = (uint64_t) seed;
    mixed ^= (uint64_t) ((int64_t) x * 341873128712LL);
    mixed ^= (uint64_t) ((int64_t) y * 132897987541LL);
    mixed ^= (uint64_t) ((int64_t) z * 42317861LL);
    mixed ^= (mixed >> 33);
    mixed *= 0xff51afd7ed558ccdULL;
    mixed ^= (mixed >> 33);
    mixed *= 0xc4ceb9fe1a85ec53ULL;
    mixed ^= (mixed >> 33);
    return (int64_t) mixed;
}

bool Aquifer::_isLava(const BlockState &state) {
    return state.mName == lavaState().mName;
}

bool Aquifer::_isWater(const BlockState &state) {
    return state.mName == waterState().mName;
}

bool Aquifer::_isAir(const BlockState &state) {
    return state.mName == airState().mName;
}
