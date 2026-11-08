#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"

class Actor;
class Level;

struct ActorMoveResult {
    Vector3f mRequested;
    Vector3f mMoved;
    bool mCollidedX = false;
    bool mCollidedY = false;
    bool mCollidedZ = false;
    bool mOnGround = false;
};

// one axis at a time, optional step-up; shared by player simulation and actor physics
class ActorCollisionSystem {
public:
    // actor: the mover, its type picks per-actor block shapes
    static ActorMoveResult move(Level &level, AxisAlignedBB &box, const Vector3f &delta, float stepHeight,
                                bool wasOnGround, const Actor *actor);

private:
    static Vector3f _resolve(Level &level, AxisAlignedBB &box, const Vector3f &delta, const Actor *actor);
};
