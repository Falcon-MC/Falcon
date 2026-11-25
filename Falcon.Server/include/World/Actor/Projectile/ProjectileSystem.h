#pragma once

#include "Core/Math/Vector3f.h"

struct ProjectileComponent;

// the flight step of the game's ProjectileSystem: the projectile moves with the motion it starts the tick with,
// turns toward that motion, and only then slows down and falls
class ProjectileSystem {
public:
    // share of the turn toward the motion made each tick
    static constexpr float ROTATION_EASING = 0.2f;

    // the motion of the next tick: inertia, or liquid_inertia when the tick began in water, on every axis, then
    // gravity on y
    static Vector3f dragAndFall(const Vector3f &motion, const ProjectileComponent &settings, bool wasInWater);

    // pitch, yaw and head yaw turned from the rotation the tick began with toward the motion it moved with
    static Vector3f face(const Vector3f &rotation, const Vector3f &motion);
};
