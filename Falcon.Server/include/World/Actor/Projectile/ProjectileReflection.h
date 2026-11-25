#pragma once

#include "Core/Math/Vector3f.h"

class Level;
class ServerActor;
class ServerNetworkHandler;

// minecraft:reflect_projectiles: a listed projectile that reaches the entity is sent back instead of hitting it
class ProjectileReflection {
public:
    // true when the entity turned the projectile away
    static bool reflect(ServerNetworkHandler &owner, Level &level, ServerActor &projectile, ServerActor &target);

    // the velocity turned by the azimuth around the vertical, raised by the elevation, scaled
    static Vector3f turned(const Vector3f &velocity, float azimuthDegrees, float elevationDegrees, float scale);
};
