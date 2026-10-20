#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class DensityOreVeins {
public:
    static DensityFunctionPtr overworldVeinToggle(const NormalNoisePtr &oreVeininess);

    static DensityFunctionPtr overworldVeinRidged(const NormalNoisePtr &oreVeinA, const NormalNoisePtr &oreVeinB);

    static DensityFunctionPtr overworldVeinGap(const NormalNoisePtr &oreGap);
};
