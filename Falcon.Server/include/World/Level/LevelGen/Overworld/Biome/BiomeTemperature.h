#pragma once

#include <cstdint>

// the temperature the game reads at a block of a biome, with the frozen ocean patches and the cooling with height
// (sub_145D3B680)
class BiomeTemperature {
public:
    static float at(int32_t biomeId, int32_t x, int32_t y, int32_t z);
};
