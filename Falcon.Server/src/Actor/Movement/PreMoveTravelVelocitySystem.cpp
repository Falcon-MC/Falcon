#include "Actor/Movement/PreMoveTravelVelocitySystem.h"

#include "Actor/Movement/ActorPushSystem.h"
#include "Actor/ServerActor.h"
#include "Block/Block.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const float GROUND_PROBE_DEPTH = 1.0f;

    float snap(float value) {
        return std::fabs(value) < PreMoveTravelVelocitySystem::PRECISION ? 0.0f : value;
    }
}

Vector3f PreMoveTravelVelocitySystem::apply(ServerNetworkHandler &owner, ServerActor &actor,
                                            const PhysicsComponent &physics, const LiquidContact &feet,
                                            bool eyesInWater) {
    Vector3f motion = actor.getMotion();

    _applyGravity(motion, physics, feet);

    if (physics.mPushable) {
        const Vector3f push = ActorPushSystem::computePush(owner, actor, motion);
        motion.x += push.x;
        motion.z += push.z;
    }

    _applyFloating(motion, physics, feet, eyesInWater);
    _applyCurrent(motion, feet);
    _applyGroundFriction(owner.getLevelFor(actor), actor, motion);
    _applyPassableFriction(motion, physics, feet);

    return motion;
}

void PreMoveTravelVelocitySystem::_applyGravity(Vector3f &motion, const PhysicsComponent &physics,
                                                const LiquidContact &feet) {
    if (physics.mHasGravity && !feet.bubble && !(physics.mSwims && feet.water))
        motion.y -= physics.mGravity;
}

void PreMoveTravelVelocitySystem::_applyFloating(Vector3f &motion, const PhysicsComponent &physics,
                                                 const LiquidContact &feet, bool eyesInWater) {
    if (!physics.mHasGravity || !physics.mFloatsInLiquid || physics.mSwims || !feet.water || feet.bubble)
        return;

    motion.y += physics.mGravity * (eyesInWater ? SUBMERGED_FLOATING_FACTOR : SURFACE_FLOATING_FACTOR);
}

void PreMoveTravelVelocitySystem::_applyCurrent(Vector3f &motion, const LiquidContact &feet) {
    const Vector3f &flow = feet.flow;
    const float length = std::sqrt(flow.x * flow.x + flow.y * flow.y + flow.z * flow.z);
    if (!(length > 0.0f))
        return;

    motion.x += flow.x / length * CURRENT_STRENGTH;
    motion.y += flow.y / length * CURRENT_STRENGTH;
    motion.z += flow.z / length * CURRENT_STRENGTH;
}

void PreMoveTravelVelocitySystem::_applyGroundFriction(Level &level, const ServerActor &actor, Vector3f &motion) {
    if (!actor.isOnGround())
        return;

    if (std::fabs(motion.x) < PRECISION && std::fabs(motion.z) < PRECISION)
        return;

    const Vector3f position = actor.getPosition();
    const BlockState *below = level.peekBlockPtr((int32_t) std::floor(position.x),
                                                 (int32_t) std::floor(position.y - GROUND_PROBE_DEPTH),
                                                 (int32_t) std::floor(position.z));
    if (below == nullptr)
        return;

    float factor = Block(*below).getFrictionFactor();
    if (factor > 0.0f && factor < 1.0f)
        factor = std::pow(factor, GROUND_FRICTION_EXPONENT);

    motion.x = snap(motion.x * factor);
    motion.z = snap(motion.z * factor);
}

void PreMoveTravelVelocitySystem::_applyPassableFriction(Vector3f &motion, const PhysicsComponent &physics,
                                                         const LiquidContact &feet) {
    if (std::fabs(motion.x) < PRECISION && std::fabs(motion.y) < PRECISION && std::fabs(motion.z) < PRECISION)
        return;

    const float waterFriction = physics.mSwims ? SWIMMER_WATER_FRICTION : WATER_FRICTION;
    const float factor = feet.lava ? LAVA_FRICTION : feet.water ? waterFriction : AIR_FRICTION;

    motion.x = snap(motion.x * factor);
    if (!feet.bubble)
        motion.y = snap(motion.y * factor);
    motion.z = snap(motion.z * factor);
}
