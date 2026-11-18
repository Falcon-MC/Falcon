#pragma once

#include <cstdint>

// the temperature the game reads at a block of a biome, with the frozen ocean patches and the cooling with height
class BiomeTemperature {
public:
    static float at(int32_t biomeId, int32_t x, int32_t y, int32_t z);

    // the variant some features read, with the column offset taken straight from its noise instead of the chunk's
    // rounded copy
    static float forFeature(int32_t biomeId, int32_t x, int32_t y, int32_t z);

private:
    static float _temperature(int32_t biomeId, int32_t x, int32_t y, int32_t z, bool rounded);
};
