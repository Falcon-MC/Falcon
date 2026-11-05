#pragma once

#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

class DensityNether {
public:
    static DensityFunctionPtr base3dNoise(const XoroshiroPositionalFactory &noiseFactory);

    static DensityFunctionPtr finalDensity(DensityFunctionPtr base3dNoise);
};
