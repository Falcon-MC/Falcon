#pragma once

#include <cstdint>
#include "World/Actor/Movement/ActorCollisionSystem.h"

struct PlayerTravelComponent {
    bool isStuck = false;
    float frictionModifier = 1.0f;
    float friction = 0.0f;
    float acceleration = 0.0f;
    Vector3f accepted;
    AxisAlignedBB box;
    ActorMoveResult moved;
};
