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

class ActorCollisionSystem {
public:
    static ActorMoveResult move(Level &level, AxisAlignedBB &box, const Vector3f &delta, float stepHeight,
                                bool wasOnGround);

private:
    static Vector3f _resolve(Level &level, AxisAlignedBB &box, const Vector3f &delta);
};
