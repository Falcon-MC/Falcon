#include "World/Actor/AI/Goal/OcelotAttackGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    Vector3i blockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

OcelotAttackGoal::OcelotAttackGoal(Settings settings)
        : MeleeAttackGoal(settings.mWalkSpeed, settings.mMaxDistance, settings.mCoolDownTicks, 0.0f),
          mSettings(settings) {}

// at the end of its walk it goes on only while it could start anew
bool OcelotAttackGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || !target->isAlive()
        || mob.distanceSquaredTo(*target) > mSettings.mMaxDistance * mSettings.mMaxDistance)
        return false;
    return !mob.getNavigation().isDone() || canUse(owner, mob);
}

// the first strike needs no wait
void OcelotAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    MeleeAttackGoal::start(owner, mob);
    mTicksSinceAttack = mCoolDown;
    mGait = Gait::None;
}

// the target is dropped: it would start the goal again right away from out of range
void OcelotAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    MeleeAttackGoal::stop(owner, mob);
    _setGait(owner, mob, Gait::None);
    mob.clearTarget();
}

void OcelotAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    const Vector3f targetPosition = target->getPosition();
    mob.getLookControl().setLookAt(targetPosition);

    const float reach = mob.getSize().mWidth * mSettings.mReachMultiplier;
    const float reachSquared = reach * reach;
    const float distance = mob.distanceSquaredTo(*target);
    Gait gait = Gait::Walk;
    if (distance > reachSquared && distance < mSettings.mMaxSprintRange * mSettings.mMaxSprintRange)
        gait = Gait::Sprint;
    else if (distance < mSettings.mMaxSneakRange * mSettings.mMaxSneakRange)
        gait = Gait::Sneak;

    const Vector3i targetBlock = blockOf(targetPosition);
    if (gait != mGait || targetBlock != mLastTargetBlock || mob.getNavigation().isDone()) {
        const float speed = gait == Gait::Sprint  ? mSettings.mSprintSpeed
                            : gait == Gait::Sneak ? mSettings.mSneakSpeed
                                                  : mSettings.mWalkSpeed;
        mob.getNavigation().moveTo(targetPosition, speed);
        mLastTargetBlock = targetBlock;
    }
    _setGait(owner, mob, gait);

    if (mTicksSinceAttack < mCoolDown)
        ++mTicksSinceAttack;
    if (distance <= reachSquared && mTicksSinceAttack >= mCoolDown)
        _attack(owner, mob, *target);
}

// viewers crouch the cat while it stalks and stretch it while it sprints
void OcelotAttackGoal::_setGait(ServerNetworkHandler &owner, MobActor &mob, Gait gait) {
    mGait = gait;
    mob.setSneakingFlag(owner, gait == Gait::Sneak);
    mob.setSprintingFlag(owner, gait == Gait::Sprint);
}
