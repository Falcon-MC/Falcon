#include "World/Level/LevelGen/Overworld/Biome/VanillaOverworldBiomeSource.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"

#include <cmath>
#include <utility>

VanillaOverworldBiomeSource::VanillaOverworldBiomeSource(DensityFunctionPtr continents, DensityFunctionPtr erosion,
                                                         DensityFunctionPtr ridges, NormalNoisePtr temperatureNoise,
                                                         NormalNoisePtr humidityNoise)
        : mContinents(std::move(continents)),
          mErosion(std::move(erosion)),
          mRidges(std::move(ridges)),
          mTemperatureNoise(std::move(temperatureNoise)),
          mHumidityNoise(std::move(humidityNoise)) {
}

OverworldBiomeResult VanillaOverworldBiomeSource::pickRaw(int32_t x, int32_t y, int32_t z,
                                                          FunctionContext &point) const {
    (void) y;

    const float continental = (float) mContinents->compute(point);
    const float temperature = mTemperatureNoise->getValue((double) x, (double) SEA_LEVEL, (double) z);
    const float humidity = mHumidityNoise->getValue((double) x, (double) SEA_LEVEL, (double) z);
    const float erosion = (float) mErosion->compute(point);
    const float weirdness = (float) mRidges->compute(point);
    const float pv = -3.0f * (-(1.0f / 3.0f) + std::fabs(-(2.0f / 3.0f) + std::fabs(weirdness)));

    const int32_t continentalLevel = continental < -1.05f
                                     ? 0
                                     : (continental < -0.455f
                                        ? 1
                                        : ((double) continental < -0.19
                                           ? 2
                                           : ((double) continental < -0.11
                                              ? 3
                                              : ((double) continental < 0.03
                                                 ? 4
                                                 : ((double) continental < 0.3 ? 5 : 6)))));
    const int32_t temperatureLevel = temperature < -0.45f
                                     ? 0
                                     : (temperature < -0.15f
                                        ? 1
                                        : (temperature < 0.3f ? 2 : (temperature < 0.55f ? 3 : 4)));
    const int32_t humidityLevel = humidity < -0.35f
                                  ? 0
                                  : (humidity < -0.1f ? 1 : (humidity < 0.1f ? 2 : (humidity < 0.3f ? 3 : 4)));
    const int32_t erosionLevel = erosion < -0.78f
                                 ? 0
                                 : (erosion < -0.375f
                                    ? 1
                                    : (erosion < -0.2225f
                                       ? 2
                                       : (erosion < 0.05f ? 3 : (erosion < 0.45f ? 4 : (erosion < 0.55f ? 5 : 6)))));

    int32_t biome;
    switch (continentalLevel) {
        case CONTINENT_MUSHROOM:
            biome = BiomeIds::MUSHROOM_ISLAND;
            break;
        case CONTINENT_OCEAN:
        case CONTINENT_DEEP_OCEAN:
            biome = getNonInlandBiome(temperatureLevel, continentalLevel);
            break;
        default:
            biome = getInlandBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);
            break;
    }

    return OverworldBiomeResult(biome, continental, temperature, humidity, erosion, weirdness, pv);
}

OverworldBiomeResult VanillaOverworldBiomeSource::pickRaw(int32_t x, int32_t y, int32_t z) const {
    MutableFunctionContext point;
    point.set(x, y, z);
    return pickRaw(x, y, z, point);
}

OverworldBiomeResult VanillaOverworldBiomeSource::pick(int32_t x, int32_t y, int32_t z, FunctionContext &point,
                                                       int32_t heightMapY) const {
    OverworldBiomeResult result = pickRaw(x, y, z, point);
    result.correct(y - heightMapY);
    return result;
}

int32_t VanillaOverworldBiomeSource::pickBiomeId(int32_t x, int32_t y, int32_t z) const {
    return pickRaw(x, y, z).getBiomeId();
}

int32_t VanillaOverworldBiomeSource::getNonInlandBiome(int32_t temperatureLevel, int32_t continentalLevel) const {
    switch (temperatureLevel) {
        case 0:
            return continentalLevel == CONTINENT_OCEAN ? BiomeIds::FROZEN_OCEAN : BiomeIds::DEEP_FROZEN_OCEAN;
        case 1:
            return continentalLevel == CONTINENT_OCEAN ? BiomeIds::COLD_OCEAN : BiomeIds::DEEP_COLD_OCEAN;
        case 2:
            return continentalLevel == CONTINENT_OCEAN ? BiomeIds::OCEAN : BiomeIds::DEEP_OCEAN;
        case 3:
            return continentalLevel == CONTINENT_OCEAN ? BiomeIds::LUKEWARM_OCEAN : BiomeIds::DEEP_LUKEWARM_OCEAN;
        default:
            return BiomeIds::WARM_OCEAN;
    }
}

int32_t VanillaOverworldBiomeSource::getInlandBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                    int32_t continentalLevel, int32_t erosionLevel,
                                                    float weirdness) const {
    if (weirdness < -0.93333334f)
        return _pickMidSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < -0.7666667f)
        return _pickHighSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < -0.56666666f)
        return _pickPeaksBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < -0.4f)
        return _pickHighSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < -0.26666668f)
        return _pickMidSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < -0.05f)
        return _pickLowSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < 0.05f)
        return _pickValleysBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < 0.26666668f)
        return _pickLowSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < 0.4f)
        return _pickMidSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < 0.56666666f)
        return _pickHighSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < 0.7666667f)
        return _pickPeaksBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    if (weirdness < 0.93333334f)
        return _pickHighSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);

    return _pickMidSliceBiome(temperatureLevel, humidityLevel, continentalLevel, erosionLevel, weirdness);
}

int32_t VanillaOverworldBiomeSource::_pickPeaksBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                     int32_t continentalLevel, int32_t erosionLevel,
                                                     float weirdness) const {
    const bool weird = weirdness >= 0.0f;
    const int32_t middleBiome = getMiddleBiome(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHot = _getMiddleBiomeOrBadlandsIfHot(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHotOrSlopeIfCold =
            _getMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(temperatureLevel, humidityLevel, weird);
    const int32_t plateauBiome = getPlateauBiome(temperatureLevel, humidityLevel, weird);
    const int32_t shatteredBiome = getShatteredBiome(temperatureLevel, humidityLevel, weird);
    const int32_t shatteredBiomeOrWindsweptSavanna =
            _maybePickWindsweptSavannaBiome(temperatureLevel, humidityLevel, weird, shatteredBiome);
    const int32_t peakBiome = _getPeakBiome(temperatureLevel, humidityLevel, weird);

    if (_isCoastToFar(continentalLevel) && erosionLevel == 0)
        return peakBiome;

    if (_isCoastToNear(continentalLevel) && erosionLevel == 1)
        return middleBiomeOrBadlandsIfHotOrSlopeIfCold;

    if (_isMidToFar(continentalLevel) && erosionLevel == 1)
        return peakBiome;

    if (_isCoastToNear(continentalLevel) && (erosionLevel == 2 || erosionLevel == 3))
        return middleBiome;

    if (_isMidToFar(continentalLevel) && erosionLevel == 2)
        return plateauBiome;

    if (_isMid(continentalLevel) && erosionLevel == 3)
        return middleBiomeOrBadlandsIfHot;

    if (_isFar(continentalLevel) && erosionLevel == 3)
        return plateauBiome;

    if (_isCoastToFar(continentalLevel) && erosionLevel == 4)
        return middleBiome;

    if (_isCoastToNear(continentalLevel) && erosionLevel == 5)
        return shatteredBiomeOrWindsweptSavanna;

    if (_isMidToFar(continentalLevel) && erosionLevel == 5)
        return shatteredBiome;

    return middleBiome;
}

int32_t VanillaOverworldBiomeSource::_pickHighSliceBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                         int32_t continentalLevel, int32_t erosionLevel,
                                                         float weirdness) const {
    const bool weird = weirdness >= 0.0f;
    const int32_t middleBiome = getMiddleBiome(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHot = _getMiddleBiomeOrBadlandsIfHot(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHotOrSlopeIfCold =
            _getMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(temperatureLevel, humidityLevel, weird);
    const int32_t plateauBiome = getPlateauBiome(temperatureLevel, humidityLevel, weird);
    const int32_t shatteredBiome = getShatteredBiome(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrWindsweptSavanna =
            _maybePickWindsweptSavannaBiome(temperatureLevel, humidityLevel, weird, middleBiome);
    const int32_t slopeBiome = _getSlopeBiome(temperatureLevel, humidityLevel, weird);
    const int32_t peakBiome = _getPeakBiome(temperatureLevel, humidityLevel, weird);

    if (_isCoast(continentalLevel) && (erosionLevel == 0 || erosionLevel == 1))
        return middleBiome;

    if (_isNear(continentalLevel) && erosionLevel == 0)
        return slopeBiome;

    if (_isMidToFar(continentalLevel) && erosionLevel == 0)
        return peakBiome;

    if (_isNear(continentalLevel) && erosionLevel == 1)
        return middleBiomeOrBadlandsIfHotOrSlopeIfCold;

    if (_isMidToFar(continentalLevel) && erosionLevel == 1)
        return slopeBiome;

    if (_isCoastToNear(continentalLevel) && (erosionLevel == 2 || erosionLevel == 3))
        return middleBiome;

    if (_isMidToFar(continentalLevel) && erosionLevel == 2)
        return plateauBiome;

    if (_isMid(continentalLevel) && erosionLevel == 3)
        return middleBiomeOrBadlandsIfHot;

    if (_isFar(continentalLevel) && erosionLevel == 3)
        return plateauBiome;

    if (_isCoastToFar(continentalLevel) && erosionLevel == 4)
        return middleBiome;

    if (_isCoastToNear(continentalLevel) && erosionLevel == 5)
        return middleBiomeOrWindsweptSavanna;

    if (_isMidToFar(continentalLevel) && erosionLevel == 5)
        return shatteredBiome;

    return middleBiome;
}

int32_t VanillaOverworldBiomeSource::_pickMidSliceBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                        int32_t continentalLevel, int32_t erosionLevel,
                                                        float weirdness) const {
    const bool weird = weirdness >= 0.0f;
    if (_isCoast(continentalLevel) && erosionLevel <= 2)
        return BiomeIds::STONE_BEACH;

    if ((temperatureLevel == 1 || temperatureLevel == 2) && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return BiomeIds::SWAMPLAND;

    if ((temperatureLevel == 3 || temperatureLevel == 4) && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return BiomeIds::MANGROVE_SWAMP;

    const int32_t middleBiome = getMiddleBiome(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHot = _getMiddleBiomeOrBadlandsIfHot(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHotOrSlopeIfCold =
            _getMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(temperatureLevel, humidityLevel, weird);
    const int32_t shatteredBiome = getShatteredBiome(temperatureLevel, humidityLevel, weird);
    const int32_t plateauBiome = getPlateauBiome(temperatureLevel, humidityLevel, weird);
    const int32_t beachBiome = getBeachBiome(temperatureLevel);
    const int32_t middleBiomeOrWindsweptSavanna =
            _maybePickWindsweptSavannaBiome(temperatureLevel, humidityLevel, weird, middleBiome);
    const int32_t shatteredCoastBiome = _pickShatteredCoastBiome(temperatureLevel, humidityLevel, weird);
    const int32_t slopeBiome = _getSlopeBiome(temperatureLevel, humidityLevel, weird);

    if (_isNearToFar(continentalLevel) && erosionLevel == 0)
        return slopeBiome;

    if (_isNearToMid(continentalLevel) && erosionLevel == 1)
        return middleBiomeOrBadlandsIfHotOrSlopeIfCold;

    if (_isFar(continentalLevel) && erosionLevel == 1)
        return temperatureLevel == 0 ? slopeBiome : plateauBiome;

    if (_isNear(continentalLevel) && erosionLevel == 2)
        return middleBiome;

    if (_isMid(continentalLevel) && erosionLevel == 2)
        return middleBiomeOrBadlandsIfHot;

    if (_isFar(continentalLevel) && erosionLevel == 2)
        return plateauBiome;

    if (_isCoastToNear(continentalLevel) && erosionLevel == 3)
        return middleBiome;

    if (_isMidToFar(continentalLevel) && erosionLevel == 3)
        return middleBiomeOrBadlandsIfHot;

    if (erosionLevel == 4) {
        if (!weird) {
            if (_isCoast(continentalLevel))
                return beachBiome;

            if (_isNearToFar(continentalLevel))
                return middleBiome;
        } else if (_isCoastToFar(continentalLevel)) {
            return middleBiome;
        }
    }

    if (_isCoast(continentalLevel) && erosionLevel == 5)
        return shatteredCoastBiome;

    if (_isNear(continentalLevel) && erosionLevel == 5)
        return middleBiomeOrWindsweptSavanna;

    if (_isMidToFar(continentalLevel) && erosionLevel == 5)
        return shatteredBiome;

    if (_isCoast(continentalLevel) && erosionLevel == 6)
        return weird ? middleBiome : beachBiome;

    if (temperatureLevel == 0 && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return middleBiome;

    return middleBiome;
}

int32_t VanillaOverworldBiomeSource::_pickLowSliceBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                        int32_t continentalLevel, int32_t erosionLevel,
                                                        float weirdness) const {
    const bool weird = weirdness >= 0.0f;
    if (_isCoast(continentalLevel) && erosionLevel <= 2)
        return BiomeIds::STONE_BEACH;

    if ((temperatureLevel == 1 || temperatureLevel == 2) && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return BiomeIds::SWAMPLAND;

    if ((temperatureLevel == 3 || temperatureLevel == 4) && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return BiomeIds::MANGROVE_SWAMP;

    const int32_t middleBiome = getMiddleBiome(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHot = _getMiddleBiomeOrBadlandsIfHot(temperatureLevel, humidityLevel, weird);
    const int32_t middleBiomeOrBadlandsIfHotOrSlopeIfCold =
            _getMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(temperatureLevel, humidityLevel, weird);
    const int32_t beachBiome = getBeachBiome(temperatureLevel);
    const int32_t middleBiomeOrWindsweptSavanna =
            _maybePickWindsweptSavannaBiome(temperatureLevel, humidityLevel, weird, middleBiome);
    const int32_t shatteredCoastBiome = _pickShatteredCoastBiome(temperatureLevel, humidityLevel, weird);

    if (_isNear(continentalLevel) && (erosionLevel == 0 || erosionLevel == 1))
        return middleBiomeOrBadlandsIfHot;

    if (_isMidToFar(continentalLevel) && (erosionLevel == 0 || erosionLevel == 1))
        return middleBiomeOrBadlandsIfHotOrSlopeIfCold;

    if (_isNear(continentalLevel) && (erosionLevel == 2 || erosionLevel == 3))
        return middleBiome;

    if (_isMidToFar(continentalLevel) && (erosionLevel == 2 || erosionLevel == 3))
        return middleBiomeOrBadlandsIfHot;

    if (_isCoast(continentalLevel) && (erosionLevel == 3 || erosionLevel == 4))
        return beachBiome;

    if (_isNearToFar(continentalLevel) && erosionLevel == 4)
        return middleBiome;

    if (_isCoast(continentalLevel) && erosionLevel == 5)
        return shatteredCoastBiome;

    if (_isNear(continentalLevel) && erosionLevel == 5)
        return middleBiomeOrWindsweptSavanna;

    if (_isMidToFar(continentalLevel) && erosionLevel == 5)
        return middleBiome;

    if (_isCoast(continentalLevel) && erosionLevel == 6)
        return beachBiome;

    if (temperatureLevel == 0 && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return middleBiome;

    return middleBiome;
}

int32_t VanillaOverworldBiomeSource::_pickValleysBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                       int32_t continentalLevel, int32_t erosionLevel,
                                                       float weirdness) const {
    const bool weird = weirdness >= 0.0f;
    const bool frozen = temperatureLevel == 0;

    if (_isCoast(continentalLevel) && (erosionLevel == 0 || erosionLevel == 1)) {
        if (frozen)
            return weird ? BiomeIds::FROZEN_RIVER : BiomeIds::STONE_BEACH;

        return weird ? BiomeIds::RIVER : BiomeIds::STONE_BEACH;
    }

    if (_isNear(continentalLevel) && (erosionLevel == 0 || erosionLevel == 1))
        return frozen ? BiomeIds::FROZEN_RIVER : BiomeIds::RIVER;

    if (_isCoastToFar(continentalLevel) && erosionLevel >= 2 && erosionLevel <= 5)
        return frozen ? BiomeIds::FROZEN_RIVER : BiomeIds::RIVER;

    if (_isCoast(continentalLevel) && erosionLevel == 6)
        return frozen ? BiomeIds::FROZEN_RIVER : BiomeIds::RIVER;

    if ((temperatureLevel == 1 || temperatureLevel == 2) && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return BiomeIds::SWAMPLAND;

    if ((temperatureLevel == 3 || temperatureLevel == 4) && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return BiomeIds::MANGROVE_SWAMP;

    if (frozen && _isNearToFar(continentalLevel) && erosionLevel == 6)
        return BiomeIds::FROZEN_RIVER;

    if (_isMidToFar(continentalLevel) && (erosionLevel == 0 || erosionLevel == 1))
        return _getMiddleBiomeOrBadlandsIfHot(temperatureLevel, humidityLevel, weird);

    return frozen ? BiomeIds::FROZEN_RIVER : BiomeIds::RIVER;
}

int32_t VanillaOverworldBiomeSource::_getMiddleBiomeOrBadlandsIfHot(int32_t temperatureLevel, int32_t humidityLevel,
                                                                    bool weird) const {
    return temperatureLevel == 4
           ? getBadlandBiome(humidityLevel, weird)
           : getMiddleBiome(temperatureLevel, humidityLevel, weird);
}

int32_t VanillaOverworldBiomeSource::_getMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(int32_t temperatureLevel,
                                                                                 int32_t humidityLevel,
                                                                                 bool weird) const {
    if (temperatureLevel == 0)
        return _getSlopeBiome(temperatureLevel, humidityLevel, weird);

    return _getMiddleBiomeOrBadlandsIfHot(temperatureLevel, humidityLevel, weird);
}

int32_t VanillaOverworldBiomeSource::_maybePickWindsweptSavannaBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                                     bool weird, int32_t underlyingBiome) const {
    return weird && temperatureLevel > 1 && humidityLevel < 4 ? BiomeIds::SAVANNA_MUTATED : underlyingBiome;
}

int32_t VanillaOverworldBiomeSource::_pickShatteredCoastBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                              bool weird) const {
    const int32_t beachOrMiddleBiome = weird
                                       ? getMiddleBiome(temperatureLevel, humidityLevel, true)
                                       : getBeachBiome(temperatureLevel);
    return _maybePickWindsweptSavannaBiome(temperatureLevel, humidityLevel, weird, beachOrMiddleBiome);
}

int32_t VanillaOverworldBiomeSource::_getPeakBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const {
    if (temperatureLevel <= 2)
        return weird ? BiomeIds::FROZEN_PEAKS : BiomeIds::JAGGED_PEAKS;

    if (temperatureLevel == 3)
        return BiomeIds::STONY_PEAKS;

    return getBadlandBiome(humidityLevel, weird);
}

int32_t VanillaOverworldBiomeSource::_getSlopeBiome(int32_t temperatureLevel, int32_t humidityLevel, bool weird) const {
    if (temperatureLevel >= 3)
        return getPlateauBiome(temperatureLevel, humidityLevel, weird);

    return humidityLevel <= 1 ? BiomeIds::SNOWY_SLOPES : BiomeIds::GROVE;
}

bool VanillaOverworldBiomeSource::_isCoast(int32_t continentalLevel) {
    return continentalLevel == CONTINENT_COAST;
}

bool VanillaOverworldBiomeSource::_isNear(int32_t continentalLevel) {
    return continentalLevel == CONTINENT_NEAR_INLAND;
}

bool VanillaOverworldBiomeSource::_isMid(int32_t continentalLevel) {
    return continentalLevel == CONTINENT_MID_INLAND;
}

bool VanillaOverworldBiomeSource::_isFar(int32_t continentalLevel) {
    return continentalLevel == CONTINENT_FAR_INLAND;
}

bool VanillaOverworldBiomeSource::_isCoastToNear(int32_t continentalLevel) {
    return continentalLevel >= CONTINENT_COAST && continentalLevel <= CONTINENT_NEAR_INLAND;
}

bool VanillaOverworldBiomeSource::_isCoastToFar(int32_t continentalLevel) {
    return continentalLevel >= CONTINENT_COAST && continentalLevel <= CONTINENT_FAR_INLAND;
}

bool VanillaOverworldBiomeSource::_isNearToMid(int32_t continentalLevel) {
    return continentalLevel >= CONTINENT_NEAR_INLAND && continentalLevel <= CONTINENT_MID_INLAND;
}

bool VanillaOverworldBiomeSource::_isNearToFar(int32_t continentalLevel) {
    return continentalLevel >= CONTINENT_NEAR_INLAND && continentalLevel <= CONTINENT_FAR_INLAND;
}

bool VanillaOverworldBiomeSource::_isMidToFar(int32_t continentalLevel) {
    return continentalLevel >= CONTINENT_MID_INLAND && continentalLevel <= CONTINENT_FAR_INLAND;
}

int32_t VanillaOverworldBiomeSource::getBeachBiome(int32_t temperatureLevel) const {
    switch (temperatureLevel) {
        case 0:
            return BiomeIds::COLD_BEACH;
        case 1:
        case 2:
        case 3:
            return BiomeIds::BEACH;
        default:
            return BiomeIds::DESERT;
    }
}

int32_t VanillaOverworldBiomeSource::getBadlandBiome(int32_t humidityLevel, bool weird) const {
    switch (humidityLevel) {
        case 0:
        case 1:
            return weird ? BiomeIds::MESA_BRYCE : BiomeIds::MESA;
        case 2:
            return BiomeIds::MESA;
        default:
            return BiomeIds::MESA_PLATEAU_STONE;
    }
}

int32_t VanillaOverworldBiomeSource::getMiddleBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                    bool weird) const {
    switch (temperatureLevel) {
        case 0:
            switch (humidityLevel) {
                case 0:
                    return weird ? BiomeIds::ICE_PLAINS_SPIKES : BiomeIds::ICE_PLAINS;
                case 1:
                    return BiomeIds::ICE_PLAINS;
                case 2:
                    return weird ? BiomeIds::COLD_TAIGA : BiomeIds::ICE_PLAINS;
                case 3:
                    return BiomeIds::COLD_TAIGA;
                default:
                    return BiomeIds::TAIGA;
            }
        case 1:
            switch (humidityLevel) {
                case 0:
                case 1:
                    return BiomeIds::PLAINS;
                case 2:
                    return BiomeIds::FOREST;
                case 3:
                    return BiomeIds::TAIGA;
                default:
                    return weird ? BiomeIds::MEGA_TAIGA : BiomeIds::REDWOOD_TAIGA_MUTATED;
            }
        case 2:
            switch (humidityLevel) {
                case 0:
                    return weird ? BiomeIds::SUNFLOWER_PLAINS : BiomeIds::FLOWER_FOREST;
                case 1:
                    return BiomeIds::PLAINS;
                case 2:
                    return BiomeIds::FOREST;
                case 3:
                    return weird ? BiomeIds::BIRCH_FOREST_MUTATED : BiomeIds::BIRCH_FOREST;
                default:
                    return BiomeIds::ROOFED_FOREST;
            }
        case 3:
            switch (humidityLevel) {
                case 0:
                case 1:
                    return BiomeIds::SAVANNA;
                case 2:
                    return weird ? BiomeIds::PLAINS : BiomeIds::FOREST;
                case 3:
                    return weird ? BiomeIds::JUNGLE_EDGE : BiomeIds::JUNGLE;
                default:
                    return weird ? BiomeIds::BAMBOO_JUNGLE : BiomeIds::JUNGLE;
            }
        default:
            return BiomeIds::DESERT;
    }
}

int32_t VanillaOverworldBiomeSource::getPlateauBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                     bool weird) const {
    switch (temperatureLevel) {
        case 0:
            switch (humidityLevel) {
                case 0:
                    return weird ? BiomeIds::ICE_PLAINS_SPIKES : BiomeIds::ICE_PLAINS;
                case 1:
                case 2:
                    return BiomeIds::ICE_PLAINS;
                default:
                    return BiomeIds::COLD_TAIGA;
            }
        case 1:
            switch (humidityLevel) {
                case 0:
                    return weird ? BiomeIds::CHERRY_GROVE : BiomeIds::MEADOW;
                case 1:
                    return BiomeIds::MEADOW;
                case 2:
                    return weird ? BiomeIds::MEADOW : BiomeIds::FOREST;
                case 3:
                    return weird ? BiomeIds::MEADOW : BiomeIds::TAIGA;
                default:
                    return weird ? BiomeIds::MEGA_TAIGA : BiomeIds::REDWOOD_TAIGA_MUTATED;
            }
        case 2:
            switch (humidityLevel) {
                case 0:
                case 1:
                    return weird ? BiomeIds::CHERRY_GROVE : BiomeIds::MEADOW;
                case 2:
                    return weird ? BiomeIds::FOREST : BiomeIds::MEADOW;
                case 3:
                    return weird ? BiomeIds::BIRCH_FOREST : BiomeIds::MEADOW;
                default:
                    return BiomeIds::PALE_GARDEN;
            }
        case 3:
            switch (humidityLevel) {
                case 0:
                case 1:
                    return BiomeIds::SAVANNA_PLATEAU;
                case 2:
                case 3:
                    return BiomeIds::FOREST;
                default:
                    return BiomeIds::JUNGLE;
            }
        default:
            switch (humidityLevel) {
                case 0:
                case 1:
                    return weird ? BiomeIds::MESA_BRYCE : BiomeIds::MESA;
                case 2:
                    return BiomeIds::MESA;
                default:
                    return BiomeIds::MESA_PLATEAU_STONE;
            }
    }
}

int32_t VanillaOverworldBiomeSource::getShatteredBiome(int32_t temperatureLevel, int32_t humidityLevel,
                                                       bool weird) const {
    switch (temperatureLevel) {
        case 0:
        case 1:
            switch (humidityLevel) {
                case 0:
                case 1:
                    return BiomeIds::EXTREME_HILLS_MUTATED;
                case 2:
                    return BiomeIds::EXTREME_HILLS;
                default:
                    return BiomeIds::EXTREME_HILLS_PLUS_TREES;
            }
        case 2:
            switch (humidityLevel) {
                case 0:
                case 1:
                case 2:
                    return BiomeIds::EXTREME_HILLS;
                default:
                    return BiomeIds::EXTREME_HILLS_PLUS_TREES;
            }
        case 3:
            switch (humidityLevel) {
                case 0:
                case 1:
                    return BiomeIds::SAVANNA;
                case 2:
                    return weird ? BiomeIds::PLAINS : BiomeIds::FOREST;
                case 3:
                    return weird ? BiomeIds::JUNGLE_EDGE : BiomeIds::JUNGLE;
                default:
                    return weird ? BiomeIds::BAMBOO_JUNGLE : BiomeIds::JUNGLE;
            }
        default:
            return BiomeIds::DESERT;
    }
}
