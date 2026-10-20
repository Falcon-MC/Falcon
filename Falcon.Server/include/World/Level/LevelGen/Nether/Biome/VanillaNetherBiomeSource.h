#pragma once

#include "World/Level/LevelGen/Biome/BiomeSource.h"
#include "World/Level/LevelGen/Nether/Biome/NetherBiomeResult.h"
#include "World/Level/LevelGen/Noise/NormalNoise.h"
#include "World/Level/LevelGen/Random/IRandom.h"

#include <cstdint>

class VanillaNetherBiomeSource : public BiomeSource {
public:
    explicit VanillaNetherBiomeSource(IRandom &random);

    ~VanillaNetherBiomeSource() override = default;

    int32_t pickBiomeId(int32_t x, int32_t y, int32_t z) const override;

    NetherBiomeResult pick(int32_t x, int32_t y, int32_t z) const;

private:
    NormalNoisePtr mTemperatureNoise;
    NormalNoisePtr mHumidityNoise;
};
