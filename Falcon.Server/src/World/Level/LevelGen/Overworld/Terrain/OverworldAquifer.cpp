#include "World/Level/LevelGen/Overworld/Terrain/OverworldAquifer.h"

#include "World/Level/LevelGen/Noise/NoiseMath.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldClimate.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

    constexpr int32_t LAVA_FLOOR_Y = -55;
    constexpr int32_t NO_FLUID_LEVEL = -4096;

    // chunk offsets of the surface samples, in the game's order (qword_14F49A788)
    const int32_t SURFACE_SAMPLE_OFFSETS[13][2] = {
            {-2, -1}, {-1, -1}, {0, -1}, {1, -1},
            {-3, 0}, {-2, 0}, {-1, 0}, {0, 0}, {1, 0},
            {-2, 1}, {-1, 1}, {0, 1}, {1, 1}
    };

    XoroshiroPositionalFactory locationFactory(const XoroshiroPositionalFactory &noiseFactory) {
        XoroshiroRandom random = noiseFactory.hashOf("minecraft:aquifer");
        return random.forkPositional();
    }

    float similarity(int64_t first, int64_t second) {
        return static_cast<float>(std::llabs(first - second)) * -0.04f + 1.0f;
    }

}

OverworldAquiferNoises::OverworldAquiferNoises(const XoroshiroPositionalFactory &noiseFactory)
        : mBarrier(NormalNoise::create(noiseFactory, "minecraft:aquifer_barrier", -3, {1.0f})),
          mFloodedness(NormalNoise::create(noiseFactory, "minecraft:aquifer_fluid_level_floodedness", -7, {1.0f})),
          mLava(NormalNoise::create(noiseFactory, "minecraft:aquifer_lava", -1, {1.0f})),
          mSpread(NormalNoise::create(noiseFactory, "minecraft:aquifer_fluid_level_spread", -5, {1.0f})),
          mLocationFactory(locationFactory(noiseFactory)) {
}

OverworldAquifer::OverworldAquifer(const OverworldAquiferNoises &noises, const OverworldClimate &climate,
                                   int32_t seaLevel)
        : mNoises(noises), mClimate(climate), mSeaLevel(seaLevel) {
}

int64_t OverworldAquifer::_cellKey(int32_t x, int32_t y, int32_t z) {
    return (static_cast<int64_t>(x & 0x1FFFFF) << 42) | (static_cast<int64_t>(y & 0x1FFFFF) << 21)
           | static_cast<int64_t>(z & 0x1FFFFF);
}

OverworldAquifer::Location OverworldAquifer::_location(int32_t cellX, int32_t cellY, int32_t cellZ) {
    const int64_t key = _cellKey(cellX, cellY, cellZ);
    const std::unordered_map<int64_t, Location>::const_iterator found = mLocations.find(key);
    if (found != mLocations.end())
        return found->second;

    XoroshiroRandom random = mNoises.mLocationFactory.at(cellX, cellY, cellZ);
    const int32_t offsetX = random.nextExclusiveInt(10);
    const int32_t offsetY = random.nextExclusiveInt(9);
    const int32_t offsetZ = random.nextExclusiveInt(10);
    const Location location{cellX * 16 + offsetX, cellY * 12 + offsetY, cellZ * 16 + offsetZ};
    mLocations.emplace(key, location);
    return location;
}

OverworldAquifer::FluidStatus OverworldAquifer::_fluidStatus(const Location &location) {
    const int64_t key = _cellKey(NoiseMath::floorDiv(location.mX, 16), NoiseMath::floorDiv(location.mY, 12),
                                 NoiseMath::floorDiv(location.mZ, 16));
    const std::unordered_map<int64_t, FluidStatus>::const_iterator found = mStatuses.find(key);
    if (found != mStatuses.end())
        return found->second;

    const FluidStatus status = _computeFluid(location);
    mStatuses.emplace(key, status);
    return status;
}

int32_t OverworldAquifer::_surfaceLevel(int32_t quartX, int32_t quartZ) {
    const int64_t key = (static_cast<int64_t>(quartX) << 32) | static_cast<uint32_t>(quartZ);
    const std::unordered_map<int64_t, int32_t>::const_iterator found = mSurfaceLevels.find(key);
    if (found != mSurfaceLevels.end())
        return found->second;

    const int32_t level = static_cast<int16_t>(mClimate.preliminarySurfaceLevel(quartX, quartZ));
    mSurfaceLevels.emplace(key, level);
    return level;
}

OverworldAquifer::FluidStatus OverworldAquifer::_computeFluid(const Location &location) {
    const FluidStatus global{mSeaLevel, false};
    int16_t lowestSurface = 0x7FFF;
    bool surfaceFlooded = false;

    for (const int32_t *offset: SURFACE_SAMPLE_OFFSETS) {
        const int32_t surface = _surfaceLevel((location.mX + 16 * offset[0]) >> 2,
                                              (location.mZ + 16 * offset[1]) >> 2);
        const int32_t fluidTop = surface + 8;
        const bool center = offset[0] == 0 && offset[1] == 0;
        if (center && location.mY - 12 > fluidTop)
            return global;

        if ((center || location.mY + 4 > surface) && fluidTop < mSeaLevel) {
            if (location.mY + 4 > surface)
                return global;

            surfaceFlooded = surfaceFlooded || center;
        }

        if (lowestSurface >= surface)
            lowestSurface = static_cast<int16_t>(surface);
    }

    float erosion = 0.0f;
    const float offset = mClimate.offsetAt(location.mX >> 2, location.mZ >> 2, &erosion);
    const float depth = static_cast<float>(static_cast<int16_t>(location.mY)) * -0.0078125f + 1.0f + offset;
    if (erosion < -0.225f && depth > 0.9f)
        return FluidStatus{NO_FLUID_LEVEL, false};

    float flooding = 1.0f;
    if (surfaceFlooded) {
        const float t = static_cast<float>(static_cast<int16_t>(lowestSurface + 8) - location.mY) * 0.015625f;
        if (t >= 0.0f)
            flooding = t <= 1.0f ? -((1.0f - t) + -1.0f) : 1.0f;
        else
            flooding = -0.0f;
    }

    float floodedness = mNoises.mFloodedness->getValue(static_cast<float>(location.mX),
                                                        static_cast<float>(location.mY) * 0.67f,
                                                        static_cast<float>(location.mZ));
    floodedness = floodedness > 1.0f ? 1.0f : std::fmax(floodedness, -1.0f);
    if (floodedness > 1.1f * flooding + -0.3f)
        return global;

    if (flooding * 1.2f + -0.8f > floodedness)
        return FluidStatus{NO_FLUID_LEVEL, false};

    const int32_t spreadCellY = NoiseMath::floorDiv(location.mY, 40);
    float spread = mNoises.mSpread->getValue(static_cast<float>(NoiseMath::floorDiv(location.mX, 16)),
                                             static_cast<float>(spreadCellY) / 1.4f,
                                             static_cast<float>(NoiseMath::floorDiv(location.mZ, 16)));
    spread = spread * 10.0f / 3.0f;
    const int32_t truncated = static_cast<int32_t>(spread);
    const int32_t quantized = truncated - (static_cast<float>(truncated) > spread ? 1 : 0);
    const int32_t level = 3 * quantized + 8 * (5 * spreadCellY) + 20;

    const bool lava = level <= -10 && std::fabs(mNoises.mLava->getValue(
            static_cast<float>(NoiseMath::floorDiv(location.mX, 64)), static_cast<float>(spreadCellY),
            static_cast<float>(NoiseMath::floorDiv(location.mZ, 64)))) > 0.3f;
    return FluidStatus{level < lowestSurface ? level : static_cast<int32_t>(lowestSurface), lava};
}

float OverworldAquifer::_pressure(int32_t x, int32_t y, int32_t z, const FluidStatus &first,
                                  const FluidStatus &second, float &barrier, bool &barrierSampled) const {
    if (y < first.mLevel && y < second.mLevel && first.mLava != second.mLava)
        return 1.0f;

    if (first.mLevel == second.mLevel)
        return 0.0f;

    const int32_t difference = std::abs(second.mLevel - first.mLevel);
    const float middle = (static_cast<float>(y) + 0.5f) - static_cast<float>(second.mLevel + first.mLevel) * 0.5f;
    float pressure = static_cast<float>(difference) * 0.5f - std::fabs(middle);
    if (middle <= 0.0f) {
        pressure = pressure + 3.0f;
        pressure = pressure / (pressure > 0.0f ? 3.0f : 10.0f);
    } else {
        pressure = pressure / (pressure > 0.0f ? 1.5f : 2.5f);
    }

    if (std::fabs(pressure) <= 2.0f) {
        if (!barrierSampled) {
            barrier = mNoises.mBarrier->getValue(static_cast<float>(x), static_cast<float>(y) * 0.5f,
                                                 static_cast<float>(z));
            barrierSampled = true;
        }

        pressure = pressure + barrier;
    }

    return pressure;
}

OverworldAquifer::Substance OverworldAquifer::compute(int32_t x, int32_t y, int32_t z) {
    if (y <= LAVA_FLOOR_Y)
        return Substance{LAVA_FLOOR_Y + 1, 0.0f, true};

    const int32_t cellX = NoiseMath::floorDiv(x - 5, 16);
    const int32_t cellY = NoiseMath::floorDiv(y + 1, 12);
    const int32_t cellZ = NoiseMath::floorDiv(z - 5, 16);

    int64_t distances[3] = {
            std::numeric_limits<int64_t>::max(),
            std::numeric_limits<int64_t>::max(),
            std::numeric_limits<int64_t>::max()
    };
    Location closest[3] = {};
    for (int32_t offsetX = 0; offsetX <= 1; offsetX++) {
        for (int32_t offsetY = -1; offsetY <= 1; offsetY++) {
            for (int32_t offsetZ = 0; offsetZ <= 1; offsetZ++) {
                const Location location = _location(cellX + offsetX, cellY + offsetY, cellZ + offsetZ);
                const int64_t dx = static_cast<int64_t>(location.mX) - x;
                const int64_t dy = static_cast<int64_t>(location.mY) - y;
                const int64_t dz = static_cast<int64_t>(location.mZ) - z;
                const int64_t distance = dx * dx + dy * dy + dz * dz;
                if (distances[0] >= distance) {
                    closest[2] = closest[1];
                    distances[2] = distances[1];
                    closest[1] = closest[0];
                    distances[1] = distances[0];
                    closest[0] = location;
                    distances[0] = distance;
                } else if (distances[1] >= distance) {
                    closest[2] = closest[1];
                    distances[2] = distances[1];
                    closest[1] = location;
                    distances[1] = distance;
                } else if (distances[2] >= distance) {
                    closest[2] = location;
                    distances[2] = distance;
                }
            }
        }
    }

    const FluidStatus first = _fluidStatus(closest[0]);
    const float closeness = similarity(distances[0], distances[1]);

    Substance substance;
    substance.mFluidLevel = first.mLevel;
    substance.mLava = first.mLava;
    if (closeness <= -1.0f)
        return substance;

    const FluidStatus second = _fluidStatus(closest[1]);
    const FluidStatus third = _fluidStatus(closest[2]);
    const float firstThird = similarity(distances[2], distances[0]);
    const float secondThird = similarity(distances[2], distances[1]);

    float barrier = 0.0f;
    bool barrierSampled = false;
    const float firstSecondPressure = _pressure(x, y, z, first, second, barrier, barrierSampled);
    const float firstThirdPressure = _pressure(x, y, z, first, third, barrier, barrierSampled);
    const float secondThirdPressure = _pressure(x, y, z, second, third, barrier, barrierSampled);

    const float weight = std::fmax(0.0f, closeness);
    const float firstThirdTerm = std::fmax(0.0f, firstThird) * firstThirdPressure;
    const float secondThirdTerm = std::fmax(0.0f, secondThird) * secondThirdPressure;
    const float pressure = std::fmax(firstSecondPressure, std::fmax(firstThirdTerm, secondThirdTerm));
    substance.mDensity = std::fmax(0.0f, pressure * (weight + weight));
    return substance;
}
