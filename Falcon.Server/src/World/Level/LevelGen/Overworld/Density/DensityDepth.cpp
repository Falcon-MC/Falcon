#include "World/Level/LevelGen/Overworld/Density/DensityDepth.h"

#include <utility>

namespace {

    constexpr int32_t DEPTH_FROM_Y = -64;
    constexpr int32_t DEPTH_TO_Y = 320;
    constexpr double DEPTH_FROM_VALUE = 1.5;
    constexpr double DEPTH_TO_VALUE = -1.5;

}

DensityFunctionPtr DensityDepth::overworldDepth(DensityFunctionPtr offset) {
    return DensityCommon::add(
            DensityCommon::yClampedGradient(DEPTH_FROM_Y, DEPTH_TO_Y, DEPTH_FROM_VALUE, DEPTH_TO_VALUE),
            std::move(offset)
    );
}
