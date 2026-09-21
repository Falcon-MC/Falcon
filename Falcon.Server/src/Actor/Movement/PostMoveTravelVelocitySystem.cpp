#include "Actor/Movement/PostMoveTravelVelocitySystem.h"

#include "Actor/ServerActor.h"
#include "Block/Systems/LiquidBlocksFetch.h"

float PostMoveTravelVelocitySystem::apply(ServerActor &actor, const PhysicsComponent &physics,
                                          const AxisAlignedBB &box, Vector3f motion, const ActorMoveResult &result,
                                          const LiquidContact &feet) {
    actor.setPosition(Vector3f((box.mMinX + box.mMaxX) * 0.5f, box.mMinY, (box.mMinZ + box.mMaxZ) * 0.5f));

    if (result.mCollidedX)
        motion.x = 0.0f;
    if (result.mCollidedY)
        motion.y = 0.0f;
    if (result.mCollidedZ)
        motion.z = 0.0f;
    actor.setMotion(motion);
    actor.setOnGround(result.mOnGround);

    if (!physics.mHasGravity || feet.water || feet.bubble) {
        actor.resetFallDistance();
        return 0.0f;
    }

    const float y = actor.getPosition().y;
    if (!result.mOnGround) {
        if (y > actor.getHighestPosition())
            actor.setHighestPosition(y);
        actor.updateFallDistance();
        return 0.0f;
    }

    actor.updateFallDistance();
    const float damage = actor.computeFallDamage();
    actor.resetFallDistance();
    return damage;
}
