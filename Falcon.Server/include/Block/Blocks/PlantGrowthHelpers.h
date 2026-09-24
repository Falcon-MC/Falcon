#pragma once

#include "Block/BlockQuery.h"
#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class Level;

namespace PlantGrowthHelpers {
    extern const char *AGE;
    extern const char *AGE_BIT;
    extern const char *LEAF_SIZE;
    extern const char *STALK_THICKNESS;

    const int32_t MAX_AGE = 15;
    const int32_t BAMBOO_MIN_LIGHT = 9;

    const int32_t FACE_DOWN = 0;
    const int32_t FACE_UP = 1;
    const int32_t FACE_NORTH = 2;
    const int32_t FACE_SOUTH = 3;
    const int32_t FACE_WEST = 4;
    const int32_t FACE_EAST = 5;

    const int32_t HORIZONTAL_FACES[] = {FACE_NORTH, FACE_SOUTH, FACE_WEST, FACE_EAST};

    Vector3i side(const Vector3i &position, int32_t face);

    Vector3i above(const Vector3i &position);

    Vector3i below(const Vector3i &position);

    using BlockQuery::isAirAt;
    using BlockQuery::isInRange;
    using BlockQuery::stateAt;

    int32_t opposite(int32_t face);

    void resetAge(Level &level, const Vector3i &position, const BlockState &state);
}
