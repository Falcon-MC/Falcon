#pragma once

#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Random/IRandom.h"

class DensityNether {
public:
    static DensityFunctionPtr base3dNoise(IRandom &random);

    static DensityFunctionPtr finalDensity(DensityFunctionPtr base3dNoise);
};
