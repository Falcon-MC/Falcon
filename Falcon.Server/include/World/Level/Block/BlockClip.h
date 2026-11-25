#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class Actor;
class Level;

struct BlockClipHit {
    Vector3i mBlock;
    // on the face struck
    Vector3f mPosition;
    // Facing number: down, up, north, south, west, east
    int32_t mFace = 1;
};

namespace BlockClip {
    // cells the game walks at most along one segment
    constexpr int32_t MAX_STEPS = 200;

    // the first collision box the segment enters, liquids ignored; actor: whose per actor shapes count, may be null.
    // an unloaded chunk ends the walk without a hit
    bool clip(Level &level, const Vector3f &from, const Vector3f &to, const Actor *actor, BlockClipHit &hit);
}
