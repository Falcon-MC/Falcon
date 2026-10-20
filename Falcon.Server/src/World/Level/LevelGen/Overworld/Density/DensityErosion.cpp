#include "World/Level/LevelGen/Overworld/Density/DensityErosion.h"

#include <utility>

namespace {

    constexpr double EROSION_XZ_SCALE = 0.25;
    constexpr double EROSION_Y_SCALE = 0.0;

}

DensityFunctionPtr DensityErosion::overworldErosion(const NormalNoisePtr &erosion, DensityFunctionPtr shiftX,
                                                    DensityFunctionPtr shiftZ) {
    return DensityCommon::flatCache(
            DensityCommon::shiftedNoise(
                    std::move(shiftX),
                    DensityCommon::zero(),
                    std::move(shiftZ),
                    EROSION_XZ_SCALE,
                    EROSION_Y_SCALE,
                    NoiseHolder(erosion)
            )
    );
}

DensityFunctionPtr DensityErosion::overworldErosion(const NormalNoisePtr &erosion, const NormalNoisePtr &shiftNoise) {
    return overworldErosion(
            erosion,
            DensityCommon::shiftA(shiftNoise),
            DensityCommon::shiftB(shiftNoise)
    );
}
