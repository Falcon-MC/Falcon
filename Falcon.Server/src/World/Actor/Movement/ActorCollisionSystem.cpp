#include "World/Actor/Movement/ActorCollisionSystem.h"

#include "World/Level/Level.h"

#include <vector>

ActorMoveResult ActorCollisionSystem::move(Level &level, AxisAlignedBB &box, const Vector3f &delta, float stepHeight,
                                           bool wasOnGround) {
    ActorMoveResult result;
    result.mRequested = delta;

    const AxisAlignedBB original = box;
    Vector3f moved = _resolve(level, box, delta);

    const bool landing = wasOnGround || (moved.y != delta.y && delta.y < 0.0f);
    const bool blockedHorizontally = moved.x != delta.x || moved.z != delta.z;

    // step-up: replay the move lifted by the step height, then drop back down; kept only when it goes further
    // horizontally: slabs and stairs climbed, walls not
    if (stepHeight > 0.0f && landing && blockedHorizontally) {
        const AxisAlignedBB flat = box;
        const Vector3f flatMoved = moved;

        box = original;
        const Vector3f stepped = _resolve(level, box, Vector3f(delta.x, stepHeight, delta.z));
        AxisAlignedBB steppedBox = box;
        const Vector3f settle = _resolve(level, steppedBox, Vector3f(0.0f, -stepped.y, 0.0f));

        const float flatDistance = flatMoved.x * flatMoved.x + flatMoved.z * flatMoved.z;
        const float steppedDistance = stepped.x * stepped.x + stepped.z * stepped.z;

        if (steppedDistance > flatDistance) {
            box = steppedBox;
            moved = Vector3f(stepped.x, stepped.y + settle.y, stepped.z);
        } else {
            box = flat;
            moved = flatMoved;
        }
    }

    result.mMoved = moved;
    result.mCollidedX = moved.x != delta.x;
    result.mCollidedY = moved.y != delta.y;
    result.mCollidedZ = moved.z != delta.z;
    // FinalizeMoveSystem (0x140C638A0) keeps the grounded state for a zero-Y move
    result.mOnGround = (result.mCollidedY && delta.y < 0.0f)
                       || (wasOnGround && !result.mCollidedY && delta.y == 0.0f);
    return result;
}

Vector3f ActorCollisionSystem::_resolve(Level &level, AxisAlignedBB &box, const Vector3f &delta) {
    const std::vector<AxisAlignedBB> obstacles = level.getCollisionBoxes(box.addCoord(delta.x, delta.y, delta.z));

    // Y, X, Z order, each axis against the box moved on the previous ones: a corner move slides along the wall it misses
    float dy = delta.y;
    for (const AxisAlignedBB &obstacle: obstacles)
        dy = obstacle.calculateYOffset(box, dy);
    box = box.offset(0.0f, dy, 0.0f);

    float dx = delta.x;
    for (const AxisAlignedBB &obstacle: obstacles)
        dx = obstacle.calculateXOffset(box, dx);
    box = box.offset(dx, 0.0f, 0.0f);

    float dz = delta.z;
    for (const AxisAlignedBB &obstacle: obstacles)
        dz = obstacle.calculateZOffset(box, dz);
    box = box.offset(0.0f, 0.0f, dz);

    return Vector3f(dx, dy, dz);
}
