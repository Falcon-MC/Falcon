#pragma once

#include "Actor/Movement/PhysicsComponent.h"
#include "Core/Math/Vector3f.h"

class Level;
class ServerActor;
class ServerNetworkHandler;
struct LiquidContact;

class PreMoveTravelVelocitySystem {
public:
    static constexpr float PRECISION = 0.00001f;

    static constexpr float GROUND_FRICTION_EXPONENT = 0.5574929506502402f;

    static constexpr float AIR_FRICTION = 0.95f;

    static constexpr float WATER_FRICTION = 0.5f;

    static constexpr float LAVA_FRICTION = 0.3f;

    static constexpr float SWIMMER_WATER_FRICTION = 0.9f;

    static constexpr float CURRENT_STRENGTH = 0.018f;

    static constexpr float SUBMERGED_FLOATING_FACTOR = 1.3f;

    static constexpr float SURFACE_FLOATING_FACTOR = 0.7f;

    static Vector3f apply(ServerNetworkHandler &owner, ServerActor &actor, const PhysicsComponent &physics,
                          const LiquidContact &feet, bool eyesInWater);

private:
    static void _applyGravity(Vector3f &motion, const PhysicsComponent &physics, const LiquidContact &feet);

    static void _applyFloating(Vector3f &motion, const PhysicsComponent &physics, const LiquidContact &feet,
                               bool eyesInWater);

    static void _applyCurrent(Vector3f &motion, const LiquidContact &feet);

    static void _applyGroundFriction(Level &level, const ServerActor &actor, Vector3f &motion);

    static void _applyPassableFriction(Vector3f &motion, const PhysicsComponent &physics, const LiquidContact &feet);
};
