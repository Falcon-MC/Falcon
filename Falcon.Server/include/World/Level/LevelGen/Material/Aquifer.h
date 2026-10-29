#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

class Aquifer {
public:
    struct FluidStatus {
        FluidStatus();

        FluidStatus(int32_t fluidLevel, const BlockState *fluidType);

        const BlockState *at(int32_t blockY) const;

        bool operator==(const FluidStatus &other) const;

        bool operator!=(const FluidStatus &other) const;

        int32_t mFluidLevel;
        const BlockState *mFluidType;
    };

    using FluidPicker = std::function<FluidStatus(int32_t, int32_t, int32_t)>;

    static const double FLOWING_UPDATE_SIMILARITY;

    static FluidPicker overworldFluidPicker(int32_t seaLevel);

    Aquifer(int32_t chunkX, int32_t chunkZ, int64_t levelSeed, DensityChunkCache *chunkCache,
            DensityFunctionPtr barrierNoise, DensityFunctionPtr fluidLevelFloodednessNoise,
            DensityFunctionPtr fluidLevelSpreadNoise, DensityFunctionPtr lavaNoise,
            DensityFunctionPtr erosion, DensityFunctionPtr depth,
            DensityFunctionPtr preliminarySurfaceDensity, DensityFunctionPtr preliminarySurfaceUpperBound,
            int32_t minAquiferY, int32_t aquiferCellHeight, int32_t minY, int32_t yBlockSize,
            FluidPicker globalFluidPicker);

    const BlockState *computeSubstance(FunctionContext &context, double density);

    bool shouldScheduleFluidUpdate() const;

private:
    static const int32_t X_RANGE = 10;
    static const int32_t Y_RANGE = 9;
    static const int32_t Z_RANGE = 10;
    static const int32_t Y_SPACING = 12;
    static const int32_t SURFACE_LEVEL_Y_OFFSET = 8;
    static const int32_t WAY_BELOW_MIN_Y = -1000000;
    static const int32_t PRELIMINARY_SURFACE_CACHE_CAP = 64;
    static const int32_t SURFACE_SAMPLING_OFFSET_COUNT = 13;
    static const int32_t SURFACE_SAMPLING_OFFSETS_IN_CHUNKS[SURFACE_SAMPLING_OFFSET_COUNT][2];

    int32_t _getIndex(int32_t gridX, int32_t gridY, int32_t gridZ) const;

    void _preloadAquiferLocations(int32_t gridSizeY);

    double _calculatePressure(FunctionContext &context, const FluidStatus &statusClosest1,
                              const FluidStatus &statusClosest2);

    const FluidStatus &_getAquiferStatus(int32_t index);

    FluidStatus _computeFluid(int32_t x, int32_t y, int32_t z);

    static int32_t _adjustSurfaceLevel(int32_t preliminarySurfaceLevel);

    int32_t _computeSurfaceLevel(int32_t x, int32_t y, int32_t z, const FluidStatus &globalFluid,
                                 int32_t lowestPreliminarySurface, bool surfaceAtCenterIsUnderGlobalFluidLevel);

    int32_t _computeRandomizedFluidSurfaceLevel(int32_t x, int32_t y, int32_t z, int32_t lowestPreliminarySurface);

    const BlockState *_computeFluidType(int32_t x, int32_t y, int32_t z, const FluidStatus &globalFluid,
                                        int32_t fluidSurfaceLevel);

    int32_t _preliminarySurfaceLevel(int32_t worldX, int32_t worldZ);

    int32_t _maxPreliminarySurfaceLevel(int32_t minX, int32_t minZ, int32_t maxX, int32_t maxZ);

    static int32_t _gridX(int32_t blockCoord);

    static int32_t _fromGridX(int32_t gridCoord, int32_t blockOffset);

    static int32_t _gridY(int32_t blockCoord);

    static int32_t _fromGridY(int32_t gridCoord, int32_t blockOffset);

    static int32_t _gridZ(int32_t blockCoord);

    static int32_t _fromGridZ(int32_t gridCoord, int32_t blockOffset);

    static int64_t _pack(int32_t x, int32_t y, int32_t z);

    static int16_t _packOffset(int32_t x, int32_t y, int32_t z);

    static int32_t _unpackOffsetX(int32_t packed);

    static int32_t _unpackOffsetY(int32_t packed);

    static int32_t _unpackOffsetZ(int32_t packed);

    static int32_t _unpackX(int64_t packed);

    static int32_t _unpackY(int64_t packed);

    static int32_t _unpackZ(int64_t packed);

    static bool _isDeepDarkRegion(const DensityFunctionPtr &erosion, const DensityFunctionPtr &depth,
                                  FunctionContext &context);

    static double _similarity(int32_t distanceSqr1, int32_t distanceSqr2);

    static double _clampedMap(double value, double inMin, double inMax, double outMin, double outMax);

    static double _map(double value, double inMin, double inMax, double outMin, double outMax);

    static int32_t _quantize(double value, int32_t step);

    static int64_t _mixSeed(int64_t seed, int32_t x, int32_t y, int32_t z);

    static bool _isLava(const BlockState &state);

    static bool _isWater(const BlockState &state);

    static bool _isAir(const BlockState &state);

    DensityFunctionPtr mBarrierNoise;
    DensityFunctionPtr mFluidLevelFloodednessNoise;
    DensityFunctionPtr mFluidLevelSpreadNoise;
    DensityFunctionPtr mLavaNoise;
    DensityFunctionPtr mErosion;
    DensityFunctionPtr mDepth;
    DensityFunctionPtr mPreliminarySurfaceDensity;
    DensityFunctionPtr mPreliminarySurfaceUpperBound;
    std::vector<std::optional<FluidStatus>> mAquiferCache;
    std::vector<int64_t> mAquiferLocationCache;
    std::vector<int16_t> mAquiferOffsetCache;
    FluidPicker mGlobalFluidPicker;
    int32_t mSkipSamplingAboveY;
    int32_t mMinY;
    int32_t mMaxY;
    int32_t mMinGridX;
    int32_t mMinGridY;
    int32_t mMinGridZ;
    int32_t mGridSizeX;
    int32_t mGridSizeZ;
    int64_t mRandomSeed;
    int32_t mPreliminarySurfaceLowerBound;
    int32_t mPreliminarySurfaceCellHeight;
    MutableChunkCacheContext mCachedPointContext;
    std::unordered_map<int64_t, int32_t> mPreliminarySurfaceLevelCache;
    double mCachedBarrierNoise;
    bool mShouldScheduleFluidUpdate;
    XoroshiroRandom mRandom;
};
