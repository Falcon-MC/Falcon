#pragma once

#include "Core/Math/AxisAlignedBB.h"

namespace FenceHelpers {
    const float POST_MIN = 0.375f;
    const float POST_MAX = 0.625f;
    const float POST_HEIGHT = 1.5f;

    AxisAlignedBB postShape();
}
