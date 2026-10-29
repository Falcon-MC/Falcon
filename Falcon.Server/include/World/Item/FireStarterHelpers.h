#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class Level;

namespace FireStarterHelpers {
    Vector3i relativeToFace(const Vector3i &position, int32_t face);

    Vector3f centerOf(const Vector3i &position);

    bool isObsidian(Level &level, const Vector3i &position);

    bool canIgniteAgainst(Level &level, const Vector3i &target, const Vector3i &placement);
}
