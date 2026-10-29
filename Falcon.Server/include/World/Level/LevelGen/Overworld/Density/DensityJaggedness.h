#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class DensityJaggedness {
public:
    static DensityFunctionPtr overworldJaggedness(const DensityFunctionPtr &continents,
                                                  const DensityFunctionPtr &erosion, const DensityFunctionPtr &ridges,
                                                  const DensityFunctionPtr &ridgesFolded);

private:
    static DensityFunctionPtr _jaggednessSpline(const DensityFunctionPtr &continents,
                                                const DensityFunctionPtr &erosion, const DensityFunctionPtr &ridges,
                                                const DensityFunctionPtr &ridgesFolded);
};
