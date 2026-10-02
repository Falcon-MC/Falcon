#pragma once

#include "Actor/Movement/ActorCollisionSystem.h"
#include "Actor/Movement/PhysicsComponent.h"

class ServerActor;
struct LiquidContact;

class PostMoveTravelVelocitySystem {
public:
    static float apply(ServerActor &actor, const PhysicsComponent &physics, const AxisAlignedBB &box,
                       Vector3f motion, const ActorMoveResult &result, const LiquidContact &feet,
                       float &landingFallDistance);
};
