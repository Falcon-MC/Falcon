#pragma once

#include "World/Level/LevelGen/Biome/BiomeSource.h"

class TheEndBiomeSource : public BiomeSource {
public:
    TheEndBiomeSource();

    ~TheEndBiomeSource() override = default;

    int32_t pickBiomeId(int32_t x, int32_t y, int32_t z) const override;
};
