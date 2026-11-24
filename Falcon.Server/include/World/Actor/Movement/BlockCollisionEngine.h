#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;
class Level;

// the game's box against terrain: axis moves with penetration recovery, step-up, collision flags
namespace BlockCollisionEngine {

    // survives between moves: a box that stays inside a shape stops being pushed out after the second move
    struct RecoveryState {
        uint32_t mFlags = 0;
    };

    struct Result {
        Vector3f mRequested;
        Vector3f mMoved;
        AxisAlignedBB mBox;
        bool mCollidedX = false;
        bool mCollidedY = false;
        bool mCollidedZ = false;
        bool mOnGround = false;
        bool mStepped = false;
        // against an unloaded chunk the game leaves the horizontal velocity alone
        bool mKeepsHorizontalVelocity = false;
        float mPenetration = 0.0f;
    };

    // flying: a flying mover steps up without standing on the ground
    // descending: the mover climbs down through the blocks that would carry it
    Result move(Level &level, const AxisAlignedBB &start, const Vector3f &delta, float stepHeight, bool wasOnGround,
                bool flying, bool pushedToClosestSpace, bool descending, RecoveryState &recovery,
                const Actor *actor);

    // feet centre of a moved box, rounded the way the game does
    Vector3f feetOf(const AxisAlignedBB &box);
}
