#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class DensityErosion {
public:
    static DensityFunctionPtr overworldErosion(const NormalNoisePtr &erosion, DensityFunctionPtr shiftX,
                                               DensityFunctionPtr shiftZ);

    static DensityFunctionPtr overworldErosion(const NormalNoisePtr &erosion, const NormalNoisePtr &shiftNoise);
};
