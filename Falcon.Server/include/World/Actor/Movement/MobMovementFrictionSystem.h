#pragma once

#include "Core/Math/Vector3f.h"

// ground and air friction for actors with the friction and air drag modifier attributes
class MobMovementFrictionSystem {
public:
    // 1 - 0.91: the mob air drag
    static constexpr float AIR_DRAG = 0.089999974f;

    // friction is the block friction on the ground, 1 in the air
    static float factor(float friction, float frictionModifier, float airDragModifier);

    // a lava walker in lava: horizontal friction of a 0.6 block with no air drag
    static float lavaWalkFriction(float frictionModifier);

    // a lava walker in lava: vertical drag, 0.91 with uniform air drag, 0.98 with gravity, none otherwise
    static float lavaWalkDrag(bool uniform, bool gravity, float airDragModifier);

    // uniform: minecraft:uses_uniform_air_drag drags the vertical speed by the same factor
    static void apply(Vector3f &velocity, float factor, bool uniform);
};
