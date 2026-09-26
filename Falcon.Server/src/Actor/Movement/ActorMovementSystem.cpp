#include "Actor/Movement/ActorMovementSystem.h"

#include "Actor/Movement/ActorCollisionSystem.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Actor/Movement/PostMoveTravelVelocitySystem.h"
#include "Actor/Movement/PreMoveTravelVelocitySystem.h"
#include "Actor/ServerActor.h"
#include "Block/Systems/BlockContactSystem.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const float EYE_HEIGHT_RATIO = 0.85f;
}

void ActorMovementSystem::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    Level &level = owner.getLevelFor(actor);
    const PhysicsComponent physics = actor.getPhysics();
    const Vector3f position = actor.getPosition();

    const LiquidContact feet = LiquidBlocksFetch::at(level, position);
    const Vector3f eyes(position.x, position.y + actor.getSize().mHeight * EYE_HEIGHT_RATIO, position.z);
    const bool eyesInWater = LiquidBlocksFetch::at(level, eyes).water;

    const Vector3f motion = PreMoveTravelVelocitySystem::apply(owner, actor, physics, feet, eyesInWater);

    AxisAlignedBB box = ActorPushSystem::boundingBoxOf(actor);
    ActorMoveResult result;
    if (physics.mHasCollision) {
        result = ActorCollisionSystem::move(level, box, motion, physics.mStepHeight, actor.isOnGround());
    } else {
        box = box.offset(motion.x, motion.y, motion.z);
        result.mRequested = motion;
        result.mMoved = motion;
    }

    float landingFallDistance = 0.0f;
    const float fallDamage = PostMoveTravelVelocitySystem::apply(actor, physics, box, motion, result, feet,
                                                                 landingFallDistance);

    _syncMovement(owner, actor);

    BlockContactSystem::land(owner, actor, landingFallDistance);

    if (fallDamage > 0.0f)
        actor.hurt(owner, fallDamage, ActorDamageSource::environment("death.fell.accident.generic", actor.getName()));
}

void ActorMovementSystem::_syncMovement(ServerNetworkHandler &owner, ServerActor &actor) {
    if (!actor.needsMovementSync())
        return;

    owner.broadcastActorMove(actor);
    actor.markMovementSynced();
}
