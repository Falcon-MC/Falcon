#include "Actor/AI/Control/LookControl.h"

#include "Actor/Mob/MobActor.h"

#include <cmath>

namespace {
    const float RADIANS_TO_DEGREES = 57.29577951308232f;
}

void LookControl::setLookAt(const Vector3f &position) {
    mLookAt = position;
    mHasLookAt = true;
}

void LookControl::clear() {
    mHasLookAt = false;
}

void LookControl::tick(MobActor &mob) {
    Vector3f rotation = mob.getRotation();

    if (mHasLookAt) {
        const Vector3f position = mob.getPosition();
        rotation.z = yawTowards(position, mLookAt);
        if (mPitchEnabled)
            rotation.x = pitchTowards(position, mLookAt);
    }

    if (!mPitchEnabled)
        rotation.x = 0.0f;

    mob.setRotation(rotation);
}

float LookControl::yawTowards(const Vector3f &from, const Vector3f &to) {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    float yaw = std::atan2(-dx, dz) * RADIANS_TO_DEGREES;
    if (yaw < 0.0f)
        yaw += 360.0f;
    return yaw;
}

float LookControl::pitchTowards(const Vector3f &from, const Vector3f &to) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float horizontal = std::sqrt(dx * dx + dz * dz);
    if (horizontal == 0.0f && dy == 0.0f)
        return 0.0f;
    return -std::atan2(dy, horizontal) * RADIANS_TO_DEGREES;
}
