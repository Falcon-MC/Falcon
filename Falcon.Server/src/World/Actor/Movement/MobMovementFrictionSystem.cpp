#include "World/Actor/Movement/MobMovementFrictionSystem.h"

#include <algorithm>
#include <cmath>

namespace {
    const float SNAP = 0.00000011920929f;
    const float LAVA_WALK_FRICTION = 0.6f;
    const float UNIFORM_LAVA_WALK_DRAG = 0.91f;
    const float GRAVITY_LAVA_WALK_DRAG = 0.98f;

    // a drag above 1 stops the actor instead of reversing it
    float keep(float drag) {
        return drag <= 1.0f ? 1.0f - std::max(0.0f, drag) : 0.0f;
    }

    float scaled(float value, float factor) {
        return std::fabs(value) <= SNAP ? 0.0f : value * factor;
    }
}

float MobMovementFrictionSystem::factor(float friction, float frictionModifier, float airDragModifier) {
    return keep((1.0f - friction) * frictionModifier) * keep(AIR_DRAG * airDragModifier);
}

float MobMovementFrictionSystem::lavaWalkFriction(float frictionModifier) {
    return keep((1.0f - LAVA_WALK_FRICTION) * frictionModifier);
}

float MobMovementFrictionSystem::lavaWalkDrag(bool uniform, bool gravity, float airDragModifier) {
    if (!uniform && !gravity)
        return 1.0f;
    return keep((1.0f - (uniform ? UNIFORM_LAVA_WALK_DRAG : GRAVITY_LAVA_WALK_DRAG)) * airDragModifier);
}

void MobMovementFrictionSystem::apply(Vector3f &velocity, float factor, bool uniform) {
    velocity.x = scaled(velocity.x, factor);
    velocity.z = scaled(velocity.z, factor);
    if (uniform)
        velocity.y = scaled(velocity.y, factor);
}
