#include "World/Level/LevelGen/Overworld/Density/DensitySlopedCheese.h"

#include <utility>

namespace {

    constexpr double JAGGED_XZ_SCALE = 1500.0;
    constexpr double JAGGED_Y_SCALE = 0.0;

}

DensityFunctionPtr DensitySlopedCheese::overworldSlopedCheese(DensityFunctionPtr depth,
                                                              DensityFunctionPtr jaggedness,
                                                              DensityFunctionPtr factor,
                                                              DensityFunctionPtr base3dNoise,
                                                              const NormalNoisePtr &jaggedNoise) {
    DensityFunctionPtr jaggedSample = DensityCommon::halfNegative(
            DensityCommon::noise(jaggedNoise, JAGGED_XZ_SCALE, JAGGED_Y_SCALE));
    DensityFunctionPtr combinedDepth = DensityCommon::add(
            std::move(depth),
            DensityCommon::flatCache(DensityCommon::mul(std::move(jaggedness), std::move(jaggedSample)))
    );

    return DensityCommon::add(
            DensityCommon::mul(
                    DensityCommon::constant(4.0),
                    DensityCommon::quarterNegative(DensityCommon::mul(std::move(combinedDepth), std::move(factor)))
            ),
            std::move(base3dNoise)
    );
}
