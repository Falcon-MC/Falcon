#pragma once

#include "World/Actor/Movement/ActorCollisionSystem.h"
#include "Core/Math/Vector3f.h"

// the velocity a collision hands back to an actor with minecraft:bounciness
class RestitutionSystem {
public:
    // below this speed a floor contact is the gravity of a resting actor, not a fall: no bounce
    static constexpr float MIN_FLOOR_SPEED = 0.08000012f;

    static constexpr float EPSILON = 0.00000011920929f;

    // ComputeRestitutionSystem: velocity is the one the move started with; the early exits of sub_1484C9200 on
    // two engine components (+136 of the 152-byte one, bit 2 of the 24-byte one) have no Falcon counterpart
    static Vector3f compute(const Vector3f &velocity, const ActorMoveResult &result, float bounciness);

    // ComputeBlockRestitutionSystem: landingBounce from the block below, -1 cancels the actor's own bounce
    static void applyBlock(Vector3f &restitution, const Vector3f &velocity, float landingBounce);

    // ApplyGravityWithBounceCorrection: gravity for the part of the tick left after the bounce
    static float correctedGravity(float gravity, float startVelocityY, float movedY);
};
