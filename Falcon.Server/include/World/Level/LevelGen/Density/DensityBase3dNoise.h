#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"
#include "World/Level/LevelGen/Random/XoroshiroPositionalFactory.h"

class DensityBase3dNoise {
public:
    static DensityFunctionPtr oldBlendedNoise(XoroshiroRandom &random, double xzScale, double yScale, double xzFactor,
                                              double yFactor, double smearScaleMultiplier);

    static DensityFunctionPtr overworld(const XoroshiroPositionalFactory &noiseFactory);
};
