#include "World/Actor/AI/Goal/MeleeBoxAttackGoal.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/MathConstants.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"

#include <algorithm>
#include <cmath>

namespace {
    const float EYE_HEIGHT_RATIO = 0.85f;
    // a ride chain deeper than this is broken data
    const int32_t MAX_RIDE_DEPTH = 16;

    Vector3f eyesOf(const AxisAlignedBB &box, const Vector3f &position) {
        return Vector3f(position.x, box.mMinY + (box.mMaxY - box.mMinY) * EYE_HEIGHT_RATIO, position.z);
    }

    bool containsInclusive(const AxisAlignedBB &box, const Vector3f &point) {
        return box.mMinX <= point.x && point.x <= box.mMaxX && box.mMinY <= point.y && point.y <= box.mMaxY
               && box.mMinZ <= point.z && point.z <= box.mMaxZ;
    }
}

MeleeBoxAttackGoal::MeleeBoxAttackGoal(float speed, float maxRange, int32_t coolDown, float horizontalReach,
                                       float fieldOfView)
        : MeleeAttackGoal(speed, maxRange, coolDown, 0.0f), mHorizontalReach(horizontalReach),
          mFieldOfView(fieldOfView), mCoolDownLeft(coolDown) {}

bool MeleeBoxAttackGoal::isInReach(const MobActor &mob, const Actor &target, float horizontalReach) {
    const AxisAlignedBB reach = ActorPushSystem::boundingBoxOf(mob).expand(horizontalReach, 0.0f, horizontalReach);
    // strict on every face: boxes that only touch miss
    return reach.intersectsWith(ActorPushSystem::boundingBoxOf(target), 0.0f);
}

float MeleeBoxAttackGoal::angleTo(const MobActor &mob, const Actor &target) {
    const Vector3f eyes = eyesOf(ActorPushSystem::boundingBoxOf(mob), mob.getPosition());
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(target);
    const float dx = (box.mMinX + box.mMaxX) * 0.5f - eyes.x;
    const float dy = (box.mMinY + box.mMaxY) * 0.5f - eyes.y;
    const float dz = (box.mMinZ + box.mMaxZ) * 0.5f - eyes.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001f)
        return 0.0f;

    const Vector3f &rotation = mob.getRotation();
    const float yaw = rotation.z * MathConstants::DEGREES_TO_RADIANS_F;
    const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
    const float lookX = -std::sin(yaw) * std::cos(pitch);
    const float lookY = -std::sin(pitch);
    const float lookZ = std::cos(yaw) * std::cos(pitch);
    const float cosine = (lookX * dx + lookY * dy + lookZ * dz) / length;
    return std::acos(std::max(-1.0f, std::min(1.0f, cosine))) / MathConstants::DEGREES_TO_RADIANS_F;
}

void MeleeBoxAttackGoal::_tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    if (_isRiding(owner, mob, target))
        return;

    const bool inReach = isInReach(mob, target, mHorizontalReach);
    bool ready = true;
    if (mCoolDownLeft > 0)
        ready = --mCoolDownLeft == 0;
    if (!inReach || !ready || !_canHit(owner, mob, target))
        return;

    mCoolDownLeft = mCoolDown;
    _attack(owner, mob, target);
}

bool MeleeBoxAttackGoal::_isRiding(ServerNetworkHandler &owner, const MobActor &mob, const Actor &target) const {
    const Actor *rider = &target;
    for (int32_t depth = 0; depth < MAX_RIDE_DEPTH && rider->getVehicleId() != 0; ++depth) {
        if (rider->getVehicleId() == mob.getUniqueId())
            return true;
        rider = owner.getActor(rider->getVehicleId());
        if (rider == nullptr)
            return false;
    }
    return false;
}

bool MeleeBoxAttackGoal::_canHit(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) const {
    // eyes already inside the target: no sight or angle check
    const Vector3f eyes = eyesOf(ActorPushSystem::boundingBoxOf(mob), mob.getPosition());
    if (containsInclusive(ActorPushSystem::boundingBoxOf(target), eyes))
        return true;

    return mob.canSee(owner, target) && angleTo(mob, target) < mFieldOfView;
}
