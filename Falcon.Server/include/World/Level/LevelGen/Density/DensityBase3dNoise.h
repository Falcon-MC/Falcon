#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"
#include "World/Level/LevelGen/Random/IRandom.h"

class DensityBase3dNoise {
public:
    static DensityFunctionPtr oldBlendedNoise(IRandom &random, double xzScale, double yScale, double xzFactor,
                                              double yFactor, double smearScaleMultiplier);

    static DensityFunctionPtr overworld(IRandom &random);
};
