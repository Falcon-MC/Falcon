#include "World/Actor/AI/Goal/RandomGroundPosition.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Level.h"

#include <cmath>

namespace {
    const int32_t ATTEMPTS = 10;

    // the game reads air outside loaded chunks
    bool isSolid(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        return state != nullptr && BlockSupport::isSolid(*state);
    }

    int32_t roll(std::mt19937 &random, int32_t range) {
        return std::uniform_int_distribution<int32_t>(0, 2 * range)(random);
    }
}

bool RandomGroundPosition::snapToGround(Level &level, const Vector3i &start, int32_t minimumY, int32_t maximumY,
                                        Vector3i &result) {
    const int32_t x = start.x;
    const int32_t z = start.z;
    const bool startSolid = isSolid(level, x, start.y, z);
    bool aboveSolid = isSolid(level, x, start.y + 1, z);
    if (startSolid && !aboveSolid) {
        result = Vector3i(x, start.y + 1, z);
        return true;
    }

    if (start.y >= minimumY) {
        for (int32_t y = start.y;; --y) {
            const bool solid = isSolid(level, x, y, z);
            if (!aboveSolid && solid) {
                result = Vector3i(x, y + 1, z);
                return true;
            }
            aboveSolid = solid;
            if (y <= minimumY)
                break;
        }
    }

    bool belowSolid = startSolid;
    for (int32_t y = start.y; y < maximumY;) {
        ++y;
        const bool solid = isSolid(level, x, y, z);
        if (!solid && belowSolid) {
            result = Vector3i(x, y, z);
            return true;
        }
        belowSolid = solid;
    }
    return false;
}

bool RandomGroundPosition::find(Level &level, const MobActor &mob, int32_t horizontalRange, int32_t verticalRange,
                                const Vector3f *direction, std::mt19937 &random, Vector3i &result) {
    const Vector3f &position = mob.getPosition();
    const Vector3i origin((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                          (int32_t) std::floor(position.z));
    for (int32_t attempt = 0; attempt < ATTEMPTS; ++attempt) {
        // x is drawn first, then z, then y
        int32_t dx = roll(random, horizontalRange) - horizontalRange;
        int32_t dz = roll(random, horizontalRange) - horizontalRange;
        const int32_t dy = roll(random, verticalRange) - verticalRange;
        if (direction != nullptr && (float) dz * direction->z + (float) dx * direction->x < 0.0f) {
            dx = -dx;
            dz = -dz;
        }

        const Vector3i candidate(origin.x + dx, origin.y + dy, origin.z + dz);
        if (snapToGround(level, candidate, origin.y - verticalRange, origin.y + verticalRange, result))
            return true;
    }
    return false;
}
