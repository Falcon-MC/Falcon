#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

namespace BlockSupport {
    Vector3i supportOf(const Vector3i &position, int blockFace);

    bool isReplaceable(const BlockState &state);

    bool isAttachable(const BlockState &support, int blockFace);

    bool isSolidOrCauldron(const BlockState &support);
}
