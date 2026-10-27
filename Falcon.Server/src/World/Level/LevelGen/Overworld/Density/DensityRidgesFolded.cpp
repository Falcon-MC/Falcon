#include "World/Level/LevelGen/Overworld/Density/DensityRidgesFolded.h"

#include <utility>

DensityFunctionPtr DensityRidgesFolded::overworldRidgesFolded(DensityFunctionPtr ridges) {
    return DensityCommon::mul(
            DensityCommon::constant(-3.0),
            DensityCommon::add(
                    DensityCommon::constant(-0.3333333333333333),
                    DensityCommon::abs(
                            DensityCommon::add(
                                    DensityCommon::constant(-0.6666666666666666),
                                    DensityCommon::abs(std::move(ridges))
                            )
                    )
            )
    );
}
