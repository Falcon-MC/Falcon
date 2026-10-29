#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class DensityContinents {
public:
    static DensityFunctionPtr overworldContinents(const NormalNoisePtr &continentalness, DensityFunctionPtr shiftX,
                                                  DensityFunctionPtr shiftZ);

    static DensityFunctionPtr overworldContinents(const NormalNoisePtr &continentalness,
                                                  const NormalNoisePtr &shiftNoise);
};
