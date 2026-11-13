#include "World/Actor/Movement/ActorMovementSystem.h"

#include "World/Actor/Movement/ActorCollisionSystem.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Movement/BuoyancySystem.h"
#include "World/Actor/Movement/MobMovementFrictionSystem.h"
#include "World/Actor/Movement/PostMoveTravelVelocitySystem.h"
#include "World/Actor/Movement/PreMoveTravelVelocitySystem.h"
#include "World/Actor/Movement/RestitutionSystem.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/Components/BlockBehavior.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/BlockContactSystem.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const float EYE_HEIGHT_RATIO = 0.85f;
    const float CLIMB_MAX_HORIZONTAL = 0.15f;
    const float CLIMB_MAX_FALL = 0.15f;
    const float CLIMB_SPEED = 0.2f;
    const float SPEED_PROBE_DEPTH = 0.5f;
    const float GROUND_PROBE_DEPTH = 1.0f;
    // the air friction runs with the friction of no block
    const float NO_BLOCK_FRICTION = 1.0f;
    const float DEFAULT_BLOCK_FRICTION = 0.6f;
}

void ActorMovementSystem::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    Level &level = owner.getLevelFor(actor);
    const PhysicsComponent physics = actor.getPhysics();
    const Vector3f position = actor.getPosition();

    const LiquidContact feet = LiquidBlocksFetch::at(level, position);
    const Vector3f eyes(position.x, position.y + actor.getSize().mHeight * EYE_HEIGHT_RATIO, position.z);
    // only floating reads the eye probe, and only with feet in water: skip the scan otherwise
    const bool eyesInWater = feet.water && LiquidBlocksFetch::at(level, eyes).water;

    const BuoyancyContact buoyancy = BuoyancySystem::contact(level, position, physics.mBuoyancy);
    Vector3f motion = PreMoveTravelVelocitySystem::apply(owner, actor, physics, feet, eyesInWater, buoyancy.floats());

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
        result = ActorCollisionSystem::move(level, box, motion, physics.mStepHeight, actor.isOnGround(), &actor);
    } else {
        box = box.offset(motion.x, motion.y, motion.z);
        result.mRequested = motion;
        result.mMoved = motion;
    }

    float landingFallDistance = 0.0f;
    PostMoveTravelVelocitySystem::apply(actor, physics, box, motion, result, feet, landingFallDistance);
    actor.setHorizontalCollision(result.mCollidedX || result.mCollidedZ);

    float gravity = physics.mGravity;
    if (physics.mDefinitionFriction && !isStuck)
        gravity = _applyRestitution(level, actor, physics, motion, result);

    if (physics.mGravityAfterMove && !isStuck) {
        Vector3f after = actor.getMotion();
        if (buoyancy.floats()) {
            if (physics.mBuoyancy.mApplyGravity)
                after.y -= physics.mGravity;
            after.y = BuoyancySystem::floatVelocity(after.y, actor.getPosition().y, actor.getBuoyancyTimer(),
                                                    physics.mBuoyancy, buoyancy,
                                                    actor.getFlags().get(ActorFlag::Leashed));
        } else {
            PreMoveTravelVelocitySystem::applyVerticalAfterMove(after, physics, feet, gravity);
        }
        actor.setMotion(after);
    }

    if (physics.mDefinitionFriction && !isStuck && !feet.water && !feet.lava)
        _applyMovementFriction(level, actor, physics);

    if (physics.mBuoyancy.mEnabled)
        actor.setBuoyancyTimer(BuoyancySystem::advanceTimer(actor.getBuoyancyTimer(), physics.mBuoyancy,
                                                            actor.getMotion(), _random()));

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

    if (landingFallDistance > 0.0f)
        actor.causeFallDamage(owner, landingFallDistance);
}

float ActorMovementSystem::_applyRestitution(Level &level, ServerActor &actor, const PhysicsComponent &physics,
                                             const Vector3f &velocity, const ActorMoveResult &result) {
    Vector3f restitution = RestitutionSystem::compute(velocity, result, physics.mBounciness);
    if (result.mCollidedY && velocity.y < 0.0f) {
        const Vector3f position = actor.getPosition();
        const Block *below = _blockAt(level, Vector3f(position.x, position.y - GROUND_PROBE_DEPTH, position.z));
        if (below != nullptr)
            RestitutionSystem::applyBlock(restitution, velocity, below->getBehavior().getLandingBounce());
    }

    if (restitution.x == 0.0f && restitution.y == 0.0f && restitution.z == 0.0f)
        return physics.mGravity;

    // ApplyRestitutionSystem adds onto the velocity the collision zeroed
    const Vector3f current = actor.getMotion();
    actor.setMotion(Vector3f(current.x + restitution.x, current.y + restitution.y, current.z + restitution.z));
    // RequestGravityCorrectionSystem: only a vertical bounce after a real move is corrected
    if (std::fabs(restitution.y) <= RestitutionSystem::EPSILON
        || std::fabs(result.mMoved.y) <= RestitutionSystem::EPSILON)
        return physics.mGravity;

    return -RestitutionSystem::correctedGravity(physics.mGravity, velocity.y, result.mMoved.y);
}

void ActorMovementSystem::_applyMovementFriction(Level &level, ServerActor &actor, const PhysicsComponent &physics) {
    float friction = NO_BLOCK_FRICTION;
    if (actor.isOnGround()) {
        const Vector3f position = actor.getPosition();
        const Block *below = _blockAt(level, Vector3f(position.x, position.y - GROUND_PROBE_DEPTH, position.z));
        friction = below == nullptr ? DEFAULT_BLOCK_FRICTION : below->getFrictionFactor();
    }

    Vector3f velocity = actor.getMotion();
    MobMovementFrictionSystem::apply(
            velocity, MobMovementFrictionSystem::factor(friction, physics.mFrictionModifier, physics.mAirDragModifier),
            physics.mUniformAirDrag);
    actor.setMotion(velocity);
}

std::mt19937 &ActorMovementSystem::_random() {
    static std::mt19937 generator(std::random_device{}());
    return generator;
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
