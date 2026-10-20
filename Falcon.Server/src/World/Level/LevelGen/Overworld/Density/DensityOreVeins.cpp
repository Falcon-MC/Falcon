#include "World/Level/LevelGen/Overworld/Density/DensityOreVeins.h"

#include <utility>

namespace {

    constexpr int32_t MIN_VEIN_Y = -60;
    constexpr int32_t MAX_VEIN_Y = 50;
    constexpr double MIN_VEIN_Y_VALUE = MIN_VEIN_Y;
    constexpr double MAX_VEIN_Y_EXCLUSIVE = MAX_VEIN_Y + 1.0;

}

DensityFunctionPtr DensityOreVeins::overworldVeinToggle(const NormalNoisePtr &oreVeininess) {
    return DensityCommon::interpolated(
            DensityCommon::rangeChoice(
                    DensityCommon::blockY(),
                    MIN_VEIN_Y_VALUE,
                    MAX_VEIN_Y_EXCLUSIVE,
                    DensityCommon::noise(oreVeininess, 1.5, 1.5),
                    DensityCommon::zero()
            )
    );
}

DensityFunctionPtr DensityOreVeins::overworldVeinRidged(const NormalNoisePtr &oreVeinA,
                                                        const NormalNoisePtr &oreVeinB) {
    DensityFunctionPtr veinA = DensityCommon::interpolated(
            DensityCommon::rangeChoice(
                    DensityCommon::blockY(),
                    MIN_VEIN_Y_VALUE,
                    MAX_VEIN_Y_EXCLUSIVE,
                    DensityCommon::noise(oreVeinA, 4.0, 4.0),
                    DensityCommon::zero()
            )
    );
    DensityFunctionPtr veinB = DensityCommon::interpolated(
            DensityCommon::rangeChoice(
                    DensityCommon::blockY(),
                    MIN_VEIN_Y_VALUE,
                    MAX_VEIN_Y_EXCLUSIVE,
                    DensityCommon::noise(oreVeinB, 4.0, 4.0),
                    DensityCommon::zero()
            )
    );
    return DensityCommon::add(
            DensityCommon::constant(-0.08),
            DensityCommon::max(DensityCommon::abs(std::move(veinA)), DensityCommon::abs(std::move(veinB)))
    );
}

DensityFunctionPtr DensityOreVeins::overworldVeinGap(const NormalNoisePtr &oreGap) {
    return DensityCommon::noise(oreGap, 1.0, 1.0);
}
