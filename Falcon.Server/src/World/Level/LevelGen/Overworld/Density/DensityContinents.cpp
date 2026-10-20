#include "World/Level/LevelGen/Overworld/Density/DensityContinents.h"

#include <utility>

namespace {

    constexpr double CONTINENTS_XZ_SCALE = 0.25;
    constexpr double CONTINENTS_Y_SCALE = 0.0;

}

DensityFunctionPtr DensityContinents::overworldContinents(const NormalNoisePtr &continentalness,
                                                          DensityFunctionPtr shiftX, DensityFunctionPtr shiftZ) {
    return DensityCommon::flatCache(
            DensityCommon::shiftedNoise(
                    std::move(shiftX),
                    DensityCommon::zero(),
                    std::move(shiftZ),
                    CONTINENTS_XZ_SCALE,
                    CONTINENTS_Y_SCALE,
                    NoiseHolder(continentalness)
            )
    );
}

DensityFunctionPtr DensityContinents::overworldContinents(const NormalNoisePtr &continentalness,
                                                          const NormalNoisePtr &shiftNoise) {
    return overworldContinents(
            continentalness,
            DensityCommon::shiftA(shiftNoise),
            DensityCommon::shiftB(shiftNoise)
    );
}
