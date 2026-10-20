#pragma once

#include "World/Level/LevelGen/Biome/BiomeSource.h"
#include "World/Level/LevelGen/Overworld/Biome/OverworldBiomeResult.h"
#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"

#include <cstdint>

class VanillaOverworldBiomeSource : public BiomeSource {
public:
    static constexpr int32_t SEA_LEVEL = 63;

    static constexpr int32_t CONTINENT_MUSHROOM = 0;
    static constexpr int32_t CONTINENT_DEEP_OCEAN = 1;
    static constexpr int32_t CONTINENT_OCEAN = 2;
    static constexpr int32_t CONTINENT_COAST = 3;
    static constexpr int32_t CONTINENT_NEAR_INLAND = 4;
    static constexpr int32_t CONTINENT_MID_INLAND = 5;
    static constexpr int32_t CONTINENT_FAR_INLAND = 6;

    VanillaOverworldBiomeSource(DensityFunctionPtr continents, DensityFunctionPtr erosion, DensityFunctionPtr ridges,
                                NormalNoisePtr temperatureNoise, NormalNoisePtr humidityNoise);

    OverworldBiomeResult pickRaw(int32_t x, int32_t y, int32_t z, FunctionContext &point) const;

    OverworldBiomeResult pickRaw(int32_t x, int32_t y, int32_t z) const;

    OverworldBiomeResult pick(int32_t x, int32_t y, int32_t z, FunctionContext &point, int32_t heightMapY) const;

    int32_t pickBiomeId(int32_t x, int32_t y, int32_t z) const override;

protected:
    int32_t getNonInlandBiome(int32_t temperatureLevel, int32_t continentalLevel) const;

    int32_t getInlandBiome(int32_t temperatureLevel, int32_t humidityLevel, int32_t continentalLevel,
                           int32_t erosionLevel, float weirdness) const;

    int32_t getBeachBiome(int32_t temperatureLevel) const;

    int32_t getBadlandBiome(int32_t humidityLevel, bool weird) const;

    int32_t getMiddleBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const;

    int32_t getPlateauBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const;

    int32_t getShatteredBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const;

private:
    int32_t _pickPeaksBiome(int32_t temperatureLevel, int32_t humidityLevel, int32_t continentalLevel,
                            int32_t erosionLevel, float weirdness) const;

    int32_t _pickHighSliceBiome(int32_t temperatureLevel, int32_t humidityLevel, int32_t continentalLevel,
                                int32_t erosionLevel, float weirdness) const;

    int32_t _pickMidSliceBiome(int32_t temperatureLevel, int32_t humidityLevel, int32_t continentalLevel,
                               int32_t erosionLevel, float weirdness) const;

    int32_t _pickLowSliceBiome(int32_t temperatureLevel, int32_t humidityLevel, int32_t continentalLevel,
                               int32_t erosionLevel, float weirdness) const;

    int32_t _pickValleysBiome(int32_t temperatureLevel, int32_t humidityLevel, int32_t continentalLevel,
                              int32_t erosionLevel, float weirdness) const;

    int32_t _getMiddleBiomeOrBadlandsIfHot(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const;

    int32_t _getMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(int32_t temperatureLevel, int32_t humidityLevel,
                                                        bool weird) const;

    int32_t _maybePickWindsweptSavannaBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird,
                                            int32_t underlyingBiome) const;

    int32_t _pickShatteredCoastBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const;

    int32_t _getPeakBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const;

    int32_t _getSlopeBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const;

    static bool _isCoast(int32_t continentalLevel);

    static bool _isNear(int32_t continentalLevel);

    static bool _isMid(int32_t continentalLevel);

    static bool _isFar(int32_t continentalLevel);

    static bool _isCoastToNear(int32_t continentalLevel);

    static bool _isCoastToFar(int32_t continentalLevel);

    static bool _isNearToMid(int32_t continentalLevel);

    static bool _isNearToFar(int32_t continentalLevel);

    static bool _isMidToFar(int32_t continentalLevel);

    DensityFunctionPtr mContinents;
    DensityFunctionPtr mErosion;
    DensityFunctionPtr mRidges;
    NormalNoisePtr mTemperatureNoise;
    NormalNoisePtr mHumidityNoise;
};
