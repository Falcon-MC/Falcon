#pragma once

#include "World/Level/LevelGen/Biome/BiomeResult.h"

#include <cstdint>

class OverworldBiomeResult : public BiomeResult {
public:
    OverworldBiomeResult(int32_t biomeId, float continental, float temperature, float humidity, float erosion,
                         float weirdness, float pv);

    OverworldBiomeResult &correct(int32_t y);

    void reset();

    float getContinental() const {
        return mContinental;
    }

    float getTemperature() const {
        return mTemperature;
    }

    float getHumidity() const {
        return mHumidity;
    }

    float getErosion() const {
        return mErosion;
    }

    float getWeirdness() const {
        return mWeirdness;
    }

    float getPv() const {
        return mPv;
    }

    int32_t getOriginal() const {
        return mOriginal;
    }

    float mContinental;
    float mTemperature;
    float mHumidity;
    float mErosion;
    float mWeirdness;
    float mPv;
    int32_t mOriginal;
};
