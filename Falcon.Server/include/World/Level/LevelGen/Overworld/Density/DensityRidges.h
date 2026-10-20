#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class DensityRidges {
public:
    static DensityFunctionPtr overworldRidges(const NormalNoisePtr &ridge, DensityFunctionPtr shiftX,
                                              DensityFunctionPtr shiftZ);

    static DensityFunctionPtr overworldRidges(const NormalNoisePtr &ridge, const NormalNoisePtr &shiftNoise);
};
