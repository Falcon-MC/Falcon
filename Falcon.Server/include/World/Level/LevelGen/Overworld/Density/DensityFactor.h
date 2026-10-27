#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class DensityFactor {
public:
    static DensityFunctionPtr overworldFactor(const DensityFunctionPtr &continents, const DensityFunctionPtr &erosion,
                                              const DensityFunctionPtr &ridges,
                                              const DensityFunctionPtr &ridgesFolded);

private:
    static DensityFunctionPtr _factorSpline(const DensityFunctionPtr &continents, const DensityFunctionPtr &erosion,
                                            const DensityFunctionPtr &ridges, const DensityFunctionPtr &ridgesFolded);
};
