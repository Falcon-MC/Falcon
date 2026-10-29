#include "World/Level/LevelGen/Overworld/Density/DensityRidges.h"

#include <utility>

namespace {

    constexpr double RIDGES_XZ_SCALE = 0.25;
    constexpr double RIDGES_Y_SCALE = 0.0;

}

DensityFunctionPtr DensityRidges::overworldRidges(const NormalNoisePtr &ridge, DensityFunctionPtr shiftX,
                                                  DensityFunctionPtr shiftZ) {
    return DensityCommon::flatCache(
            DensityCommon::shiftedNoise(
                    std::move(shiftX),
                    DensityCommon::zero(),
                    std::move(shiftZ),
                    RIDGES_XZ_SCALE,
                    RIDGES_Y_SCALE,
                    NoiseHolder(ridge)
            )
    );
}

DensityFunctionPtr DensityRidges::overworldRidges(const NormalNoisePtr &ridge, const NormalNoisePtr &shiftNoise) {
    return overworldRidges(
            ridge,
            DensityCommon::shiftA(shiftNoise),
            DensityCommon::shiftB(shiftNoise)
    );
}
