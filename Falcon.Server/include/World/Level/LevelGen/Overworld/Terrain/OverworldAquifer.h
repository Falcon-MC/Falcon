#pragma once

#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

#include <cstdint>
#include <unordered_map>

class OverworldClimate;

// shared by every chunk's aquifer
class OverworldAquiferNoises {
public:
    explicit OverworldAquiferNoises(const XoroshiroPositionalFactory &noiseFactory);

    NormalNoisePtr mBarrier;
    NormalNoisePtr mFloodedness;
    NormalNoisePtr mLava;
    NormalNoisePtr mSpread;
    XoroshiroPositionalFactory mLocationFactory;
};

// fluids and barriers in one chunk's negative density, one source per 16x12x16 cell
class OverworldAquifer {
public:
    struct Substance {
        int32_t mFluidLevel = 0;
        float mDensity = 0.0f;
        bool mLava = false;
    };

    OverworldAquifer(const OverworldAquiferNoises &noises, const OverworldClimate &climate, int32_t seaLevel);

    // for a block whose density is not positive; the returned density is added to it
    Substance compute(int32_t x, int32_t y, int32_t z);

private:
    struct Location {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
    };

    struct FluidStatus {
        int32_t mLevel;
        bool mLava;
    };

    static int64_t _cellKey(int32_t x, int32_t y, int32_t z);

    Location _location(int32_t cellX, int32_t cellY, int32_t cellZ);

    FluidStatus _fluidStatus(const Location &location);

    FluidStatus _computeFluid(const Location &location);

    int32_t _surfaceLevel(int32_t quartX, int32_t quartZ);

    float _pressure(int32_t x, int32_t y, int32_t z, const FluidStatus &first, const FluidStatus &second,
                    float &barrier, bool &barrierSampled) const;

    const OverworldAquiferNoises &mNoises;
    const OverworldClimate &mClimate;
    int32_t mSeaLevel;
    std::unordered_map<int64_t, Location> mLocations;
    std::unordered_map<int64_t, FluidStatus> mStatuses;
    std::unordered_map<int64_t, int32_t> mSurfaceLevels;
};
