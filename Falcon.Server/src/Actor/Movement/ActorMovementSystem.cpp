#include "Actor/Movement/ActorMovementSystem.h"

#include "Actor/Movement/ActorCollisionSystem.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Actor/Movement/PostMoveTravelVelocitySystem.h"
#include "Actor/Movement/PreMoveTravelVelocitySystem.h"
#include "Actor/ServerActor.h"
#include "Block/Block.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/BlockContactSystem.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const float EYE_HEIGHT_RATIO = 0.85f;
    const float CLIMB_MAX_HORIZONTAL = 0.15f;
    const float CLIMB_MAX_FALL = 0.15f;
    const float CLIMB_SPEED = 0.2f;
    const float SPEED_PROBE_DEPTH = 0.5f;
}

void ActorMovementSystem::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    Level &level = owner.getLevelFor(actor);
    const PhysicsComponent physics = actor.getPhysics();
    const Vector3f position = actor.getPosition();

    const LiquidContact feet = LiquidBlocksFetch::at(level, position);
    const Vector3f eyes(position.x, position.y + actor.getSize().mHeight * EYE_HEIGHT_RATIO, position.z);
    const bool eyesInWater = LiquidBlocksFetch::at(level, eyes).water;

    Vector3f motion = PreMoveTravelVelocitySystem::apply(owner, actor, physics, feet, eyesInWater);

    const bool climbing = _isClimbing(level, actor, physics);
    if (climbing) {
        motion.x = std::max(-CLIMB_MAX_HORIZONTAL, std::min(CLIMB_MAX_HORIZONTAL, motion.x));
        motion.z = std::max(-CLIMB_MAX_HORIZONTAL, std::min(CLIMB_MAX_HORIZONTAL, motion.z));
        motion.y = std::max(motion.y, -CLIMB_MAX_FALL);
        if (actor.hasHorizontalCollision())
            motion.y = CLIMB_SPEED;
        actor.resetFallDistance();
    }

    Vector3f stuck;
    const bool isStuck = actor.takeStuckMultiplier(stuck);
    if (isStuck) {
        motion.x *= stuck.x;
        motion.y *= stuck.y;
        motion.z *= stuck.z;
    }

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
    actor.setHorizontalCollision(result.mCollidedX || result.mCollidedZ);

    if (isStuck) {
        actor.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
    } else {
        const float speedFactor = _speedFactorBelow(level, actor);
        if (speedFactor != 1.0f) {
            Vector3f slowed = actor.getMotion();
            slowed.x *= speedFactor;
            slowed.z *= speedFactor;
            actor.setMotion(slowed);
        }
    }

    _syncMovement(owner, actor);

    BlockContactSystem::land(owner, actor, landingFallDistance);

    if (fallDamage > 0.0f)
        actor.hurt(owner, fallDamage, ActorDamageSource::environment("death.fell.accident.generic", actor.getName()));
}

const Block *ActorMovementSystem::_blockAt(Level &level, const Vector3f &position) {
    const BlockState *state = level.peekBlockPtr((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                                                 (int32_t) std::floor(position.z));
    return state == nullptr ? nullptr : VanillaBlocks::fromIdentifier(state->mName);
}

bool ActorMovementSystem::_isClimbing(Level &level, const ServerActor &actor, const PhysicsComponent &physics) {
    if (physics.mClimbsWalls && actor.hasHorizontalCollision())
        return true;
    if (!physics.mClimbsLadders && !physics.mClimbsWalls)
        return false;

    const Block *block = _blockAt(level, actor.getPosition());
    return block != nullptr && block->isClimbable();
}

float ActorMovementSystem::_speedFactorBelow(Level &level, const ServerActor &actor) {
    const Vector3f position = actor.getPosition();
    const Block *inside = _blockAt(level, position);
    if (inside != nullptr && inside->getSpeedFactor() != 1.0f)
        return inside->getSpeedFactor();

    if (!actor.isOnGround())
        return 1.0f;

    const Block *below = _blockAt(level, Vector3f(position.x, position.y - SPEED_PROBE_DEPTH, position.z));
    return below == nullptr ? 1.0f : below->getSpeedFactor();
}

void ActorMovementSystem::_syncMovement(ServerNetworkHandler &owner, ServerActor &actor) {
    if (!actor.needsMovementSync())
        return;

    owner.broadcastActorMove(actor);
    actor.markMovementSynced();
}
