#include "World/Level/LevelGen/Nether/Biome/VanillaNetherBiomeSource.h"

#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"

#include <cstddef>
#include <vector>

namespace {
    struct ClimatePoint {
        int32_t mBiomeId;
        float mTemperature;
        float mHumidity;
        float mAltitude;
    };

    const ClimatePoint CLIMATE_POINTS[] = {
            {NetherBiomeIds::BASALT_DELTAS,  -0.5f, 0.0f,  0.175f},
            {NetherBiomeIds::CRIMSON_FOREST, 0.4f,  0.0f,  0.0f},
            {NetherBiomeIds::HELL,           0.0f,  0.0f,  0.0f},
            {NetherBiomeIds::SOULSAND_VALLEY, 0.0f, -0.5f, 0.0f},
            {NetherBiomeIds::WARPED_FOREST,  0.0f,  0.5f,  0.375f}
    };
}

VanillaNetherBiomeSource::VanillaNetherBiomeSource(IRandom &random) {
    const IRandomPtr temperatureRandom = random.fork();
    mTemperatureNoise = std::make_shared<NormalNoise>(*temperatureRandom, -10,
                                                      std::vector<float>{1.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f});

    const IRandomPtr humidityRandom = random.fork();
    mHumidityNoise = std::make_shared<NormalNoise>(*humidityRandom, -8,
                                                   std::vector<float>{1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f});
}

NetherBiomeResult VanillaNetherBiomeSource::pick(int32_t x, int32_t y, int32_t z) const {
    const float temperature = mTemperatureNoise->getValue((double) x, (double) y, (double) z);
    const float humidity = mHumidityNoise->getValue((double) x, (double) y, (double) z);

    double bestDistance = 0.0;
    bool hasBest = false;
    int32_t biomeId = NetherBiomeIds::HELL;

    for (const ClimatePoint &point: CLIMATE_POINTS) {
        const double deltaTemperature = (double) temperature - (double) point.mTemperature;
        const double deltaHumidity = (double) humidity - (double) point.mHumidity;
        const double deltaAltitude = 0.0 - (double) point.mAltitude;
        const double delta = deltaTemperature * deltaTemperature
                             + deltaHumidity * deltaHumidity
                             + deltaAltitude * deltaAltitude;

        if (!hasBest || delta < bestDistance) {
            hasBest = true;
            bestDistance = delta;
            biomeId = point.mBiomeId;
        }
    }

    return NetherBiomeResult(biomeId, temperature, humidity);
}

int32_t VanillaNetherBiomeSource::pickBiomeId(int32_t x, int32_t y, int32_t z) const {
    return pick(x, y, z).getBiomeId();
}
