#pragma once

#include "World/Actor/Movement/ActorCollisionSystem.h"
#include "World/Actor/Movement/PhysicsComponent.h"
#include "Core/Math/Vector3f.h"

#include <random>

class Block;
class Level;
class ServerActor;
class ServerNetworkHandler;

class ActorMovementSystem {
public:
    static void tick(ServerNetworkHandler &owner, ServerActor &actor);

private:
    // the gravity of this tick: less than a full pull after a bounce
    static float _applyRestitution(Level &level, ServerActor &actor, const PhysicsComponent &physics,
                                   const Vector3f &velocity, const ActorMoveResult &result);

    static void _applyMovementFriction(Level &level, ServerActor &actor, const PhysicsComponent &physics);

    static std::mt19937 &_random();

    static const Block *_blockAt(Level &level, const Vector3f &position);

    static bool _isClimbing(Level &level, const ServerActor &actor, const PhysicsComponent &physics);

    static float _speedFactorBelow(Level &level, const ServerActor &actor);

    static void _syncMovement(ServerNetworkHandler &owner, ServerActor &actor);
};
