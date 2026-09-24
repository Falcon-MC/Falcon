#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

class Level;

namespace BlockQuery {
    BlockState stateAt(Level &level, const Vector3i &position);

    bool isAirAt(Level &level, const Vector3i &position);

    bool isInRange(Level &level, const Vector3i &position);
}
