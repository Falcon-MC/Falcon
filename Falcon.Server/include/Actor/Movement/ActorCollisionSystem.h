#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"

class Level;

struct ActorMoveResult {
    Vector3f mRequested;
    Vector3f mMoved;
    bool mCollidedX = false;
    bool mCollidedY = false;
    bool mCollidedZ = false;
    bool mOnGround = false;
};

/**
 * Moves an axis-aligned box through the level's collision boxes, one axis at a time, with
 * optional step-up. Shared by player movement simulation and actor physics.
 */
class ActorCollisionSystem {
public:
    static ActorMoveResult move(Level &level, AxisAlignedBB &box, const Vector3f &delta, float stepHeight,
                                bool wasOnGround);

private:
    static Vector3f _resolve(Level &level, AxisAlignedBB &box, const Vector3f &delta);
};
