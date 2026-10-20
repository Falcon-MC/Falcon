#pragma once

#include "World/Level/LevelGen/Biome/BiomeResult.h"

#include <cstdint>

class NetherBiomeResult : public BiomeResult {
public:
    NetherBiomeResult(int32_t biomeId, float temperature, float humidity)
            : BiomeResult(biomeId), mTemperature(temperature), mHumidity(humidity) {
    }

    float getTemperature() const {
        return mTemperature;
    }

    float getHumidity() const {
        return mHumidity;
    }

    float mTemperature;
    float mHumidity;
};
