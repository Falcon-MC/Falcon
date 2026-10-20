#pragma once

#include <cstdint>

class ClimateAttributes {
public:
    float mTemperature = 0.5f;
    float mDownfall = 0.5f;
    bool mRain = true;

    static const ClimateAttributes *getForBiome(int32_t biomeId);
};
