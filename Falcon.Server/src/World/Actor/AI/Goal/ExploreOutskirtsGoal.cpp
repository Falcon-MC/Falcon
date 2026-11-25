#include "World/Actor/AI/Goal/ExploreOutskirtsGoal.h"

#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"
#include "World/Village/Village.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <memory>
#include <random>

namespace {
    const int32_t TARGET_HEIGHT = 7;

    std::mt19937 &exploreRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

ExploreOutskirtsGoal::ExploreOutskirtsGoal(Settings settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool ExploreOutskirtsGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const MobDweller &dweller = mob.getDweller();
    const std::shared_ptr<Village> village = dweller.isActive() ? dweller.getVillage() : nullptr;
    if (village == nullptr)
        return false;

    // a ground spot the explore distance past the box, away from the centre through the villager
    const Vector3f center = village->getCenter();
    const Vector3f &position = mob.getPosition();
    float dx = position.x - center.x;
    float dz = position.z - center.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 1.0e-3f) {
        const float angle = std::uniform_real_distribution<float>(0.0f, 6.2831855f)(exploreRandom());
        dx = std::cos(angle);
        dz = std::sin(angle);
    } else {
        dx /= length;
        dz /= length;
    }
    const AxisAlignedBB &bounds = village->getBounds();
    const float halfX = (bounds.mMaxX - bounds.mMinX) * 0.5f;
    const float halfZ = (bounds.mMaxZ - bounds.mMinZ) * 0.5f;
    const float reach = std::sqrt(halfX * halfX + halfZ * halfZ) + mSettings.mExploreDistance;
    const Vector3i start((int32_t) std::floor(center.x + dx * reach), (int32_t) std::floor(position.y),
                         (int32_t) std::floor(center.z + dz * reach));

    Vector3i ground;
    if (!RandomGroundPosition::snapToGround(owner.getLevelFor(mob), start, start.y - TARGET_HEIGHT,
                                            start.y + TARGET_HEIGHT, ground))
        return false;
    mTarget = Vector3f((float) ground.x + 0.5f, (float) ground.y, (float) ground.z + 0.5f);
    return true;
}

bool ExploreOutskirtsGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (mArrived && !mReturning)
        return true;
    return !mob.getNavigation().isDone();
}

void ExploreOutskirtsGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mArrived = false;
    mReturning = false;
    mWaitTicks = 0;
    mHome = mob.getPosition();
    mob.getNavigation().moveTo(mTarget, mSettings.mSpeed);
}

void ExploreOutskirtsGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

void ExploreOutskirtsGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    const Vector3f &position = mob.getPosition();
    const float dx = position.x - mTarget.x;
    const float dz = position.z - mTarget.z;
    if (!mArrived && dx * dx + dz * dz <= mSettings.mMinDistanceFromTarget * mSettings.mMinDistanceFromTarget) {
        mArrived = true;
        mob.getNavigation().stop(mob);
    }
    if (!mArrived || mReturning)
        return;
    // back home before the goal can start again: from the outskirts it would pick the same spot
    if (++mWaitTicks >= mSettings.mMaxWaitTicks) {
        mReturning = true;
        mob.getNavigation().moveTo(mHome, mSettings.mSpeed);
    }
}
