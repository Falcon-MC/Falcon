#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class DensityDepth {
public:
    static DensityFunctionPtr overworldDepth(DensityFunctionPtr offset);
};
