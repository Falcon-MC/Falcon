#pragma once

#include "Core/Math/Vector3f.h"

// MobMovementFriction::Ground and ::Air for actors with the friction and air drag modifier attributes
class MobMovementFrictionSystem {
public:
    // 1 - 0.91: the mob air drag
    static constexpr float AIR_DRAG = 0.089999974f;

    // friction is the block friction on the ground, 1 in the air
    static float factor(float friction, float frictionModifier, float airDragModifier);

    // uniform: minecraft:uses_uniform_air_drag drags the vertical speed by the same factor
    static void apply(Vector3f &velocity, float factor, bool uniform);
};
