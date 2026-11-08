#include "World/Actor/Movement/RestitutionSystem.h"

#include <algorithm>
#include <cmath>

Vector3f RestitutionSystem::compute(const Vector3f &velocity, const ActorMoveResult &result, float bounciness) {
    // sub_1484C9200
    if (std::fabs(bounciness) <= EPSILON)
        return Vector3f(0.0f, 0.0f, 0.0f);

    Vector3f restitution(0.0f, 0.0f, 0.0f);
    if (result.mCollidedX)
        restitution.x = -velocity.x * bounciness;
    if (result.mCollidedY && (velocity.y >= 0.0f || std::fabs(velocity.y) >= MIN_FLOOR_SPEED))
        restitution.y = -velocity.y * bounciness;
    if (result.mCollidedZ)
        restitution.z = -velocity.z * bounciness;
    return restitution;
}

void RestitutionSystem::applyBlock(Vector3f &restitution, const Vector3f &velocity, float landingBounce) {
    // sub_1484BC1E0
    if (!(velocity.y < 0.0f) || std::fabs(velocity.y) < MIN_FLOOR_SPEED || std::fabs(landingBounce) <= EPSILON)
        return;

    if (landingBounce == -1.0f) {
        if (restitution.y > 0.0f)
            restitution.y = 0.0f;
        return;
    }

    restitution.y = std::max(landingBounce * -velocity.y, restitution.y);
}

float RestitutionSystem::correctedGravity(float gravity, float startVelocityY, float movedY) {
    // sub_143368B50: the move reached the floor after a fraction of the tick, gravity only acts on the rest
    const float distance = std::fabs(movedY);
    const float strength = std::fabs(gravity);
    if (distance <= EPSILON || strength <= EPSILON)
        return -gravity;

    const float sign = startVelocityY > 0.0f ? 1.0f : startVelocityY < 0.0f ? -1.0f : 0.0f;
    const float reached = std::fabs(
            (std::fabs(startVelocityY) - std::sqrt((strength + strength) * distance + startVelocityY * startVelocityY))
            / strength);
    return (1.0f - reached) * sign * strength;
}
