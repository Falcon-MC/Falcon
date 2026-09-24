#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <vector>

class Level;

class DoubleBlock {
public:
    static bool isUpperHalf(const BlockState &state);

    static std::vector<Vector3i> otherHalf(Level &level, const Vector3i &position, const BlockState &state);

    static bool isComplete(Level &level, const Vector3i &position, const BlockState &state);
};
