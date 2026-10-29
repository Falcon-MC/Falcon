#pragma once

#include <cstdint>
#include "Core/Math/Vector3f.h"

struct StateVectorComponent {
    Vector3f start;
    Vector3f simulated;
    Vector3f velocity;
    bool onGround = false;
    int32_t jumpDelay = 0;
};
