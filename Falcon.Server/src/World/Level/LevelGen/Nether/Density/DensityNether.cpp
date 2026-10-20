#include "World/Level/LevelGen/Nether/Density/DensityNether.h"

#include "World/Level/LevelGen/Density/DensityBase3dNoise.h"
#include "World/Level/LevelGen/Density/DensityCommon.h"

#include <utility>

DensityFunctionPtr DensityNether::base3dNoise(IRandom &random) {
    return DensityBase3dNoise::oldBlendedNoise(random, 0.25, 0.375, 80.0, 60.0, 8.0);
}

DensityFunctionPtr DensityNether::finalDensity(DensityFunctionPtr base3dNoise) {
    DensityFunctionPtr density = DensityCommon::add(DensityCommon::constant(-0.9375), std::move(base3dNoise));
    density = DensityCommon::mul(DensityCommon::yClampedGradient(104, 128, 1.0, 0.0), density);
    density = DensityCommon::add(DensityCommon::constant(0.9375), density);
    density = DensityCommon::add(DensityCommon::constant(-2.5), density);
    density = DensityCommon::mul(DensityCommon::yClampedGradient(-8, 24, 0.0, 1.0), density);
    density = DensityCommon::add(DensityCommon::constant(2.5), density);
    density = DensityCommon::blendDensity(density);
    density = DensityCommon::interpolated(density);
    density = DensityCommon::mul(DensityCommon::constant(0.64), density);
    return DensityCommon::squeeze(density);
}
