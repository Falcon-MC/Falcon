#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <random>

class Level;
class MobActor;

// the game's random walk target for ground mobs: ten tries around the mob, snapped to standable ground
namespace RandomGroundPosition {
    // direction: a candidate pointing away from it is mirrored; null for any direction
    bool find(Level &level, const MobActor &mob, int32_t horizontalRange, int32_t verticalRange,
              const Vector3f *direction, std::mt19937 &random, Vector3i &result);

    // a solid block with a free block above, scanned down then up within the range around the start
    bool snapToGround(Level &level, const Vector3i &start, int32_t minimumY, int32_t maximumY, Vector3i &result);
}
