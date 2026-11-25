#include "World/Actor/Projectile/ProjectileSystem.h"

#include "Core/Math/MathConstants.h"
#include "Core/Math/VectorMath.h"
#include "World/Actor/Projectile/ProjectileComponent.h"

#include <cmath>

Vector3f ProjectileSystem::dragAndFall(const Vector3f &motion, const ProjectileComponent &settings, bool wasInWater) {
    const float inertia = wasInWater ? settings.mLiquidInertia : settings.mInertia;
    return Vector3f(motion.x * inertia, motion.y * inertia - settings.mGravity, motion.z * inertia);
}

Vector3f ProjectileSystem::face(const Vector3f &rotation, const Vector3f &motion) {
    const float horizontal = std::sqrt(motion.z * motion.z + motion.x * motion.x);
    const float pitch = std::atan2(motion.y, horizontal) / MathConstants::DEGREES_TO_RADIANS_F;
    const float yaw = std::atan2(motion.x, motion.z) / MathConstants::DEGREES_TO_RADIANS_F;
    const float easedPitch = rotation.x + VectorMath::wrapDegrees(pitch - rotation.x) * ROTATION_EASING;
    const float easedYaw = rotation.y + VectorMath::wrapDegrees(yaw - rotation.y) * ROTATION_EASING;
    return Vector3f(easedPitch, easedYaw, easedYaw);
}
