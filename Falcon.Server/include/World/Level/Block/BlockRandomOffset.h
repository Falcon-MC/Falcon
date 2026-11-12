#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

// the per-position shift the game gives some blocks (bamboo, pointed dripstone): seeded by x and z alone, so a
// whole column shares it
namespace BlockRandomOffset {

    // range: furthest shift on x and z, in blocks; steps: how many values the shift can take
    Vector3f horizontal(int32_t x, int32_t z, float range, uint32_t steps);
}
