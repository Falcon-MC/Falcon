#pragma once

#include "Actor/Movement/PhysicsComponent.h"
#include "Core/Math/Vector3f.h"

class Block;
class Level;
class ServerActor;
class ServerNetworkHandler;

class ActorMovementSystem {
public:
    static void tick(ServerNetworkHandler &owner, ServerActor &actor);

private:
    static const Block *_blockAt(Level &level, const Vector3f &position);

    static bool _isClimbing(Level &level, const ServerActor &actor, const PhysicsComponent &physics);

    static float _speedFactorBelow(Level &level, const ServerActor &actor);

    static void _syncMovement(ServerNetworkHandler &owner, ServerActor &actor);
};
