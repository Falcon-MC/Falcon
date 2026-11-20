#include "World/Actor/AI/Goal/HoldGroundGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"

#include <cmath>
#include <cstring>
#include <utility>

namespace {
    const int AMBIENT_SOUND_CHANCE = 50;

    // a target it is already fighting is left to the attack goals
    bool isFighting(const MobActor &mob, const Actor &target) {
        return mob.getLastHurtBy() == target.getRuntimeId() || mob.getLastHurtMob() == target.getRuntimeId();
    }
}

HoldGroundGoal::HoldGroundGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool HoldGroundGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr || isFighting(mob, *target))
        return false;

    if (mob.distanceSquaredTo(*target) > mSettings.mMinRadius * mSettings.mMinRadius)
        mTargetId = target->getRuntimeId();

    const Actor *held = _target(owner, mob);
    return held != nullptr && held->isAlive();
}

bool HoldGroundGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = _target(owner, mob);
    if (target == nullptr || !mob.canTarget(*target) || isFighting(mob, *target))
        return false;

    return mob.distanceSquaredTo(*target) >= mSettings.mMinRadius * mSettings.mMinRadius;
}

void HoldGroundGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getNavigation().stop(mob);
    if (!mSettings.mBroadcast)
        return;

    Actor *target = _target(owner, mob);
    if (target == nullptr)
        return;

    _forEachKin(owner, mob, false, [&](MobActor &kin) {
        kin.setTarget(owner, target->getRuntimeId());
    });
}

void HoldGroundGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = _target(owner, mob);
    // the target walked in: every one around, this one too, switches to fighting
    if (mSettings.mBroadcast && target != nullptr && target->isAlive()
        && mob.distanceSquaredTo(*target) < mSettings.mMinRadius * mSettings.mMinRadius) {
        _forEachKin(owner, mob, true, [&](MobActor &kin) {
            EntityEvents::fireTrigger(owner, kin, mSettings.mWithinRadiusEvent.get());
        });
    }

    mTargetId = 0;
}

void HoldGroundGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = _target(owner, mob);
    if (target == nullptr)
        return;

    mob.getLookControl().setLookAt(target->getPosition());
    if (MobActor::randomRange(0, AMBIENT_SOUND_CHANCE - 1) == 0)
        mob.playDefinitionSound(owner, "ambient");
}

Actor *HoldGroundGoal::_target(ServerNetworkHandler &owner, const MobActor &mob) const {
    (void) mob;
    return mTargetId == 0 ? nullptr : MobActor::findActor(owner, mTargetId);
}

template<typename Visitor>
void HoldGroundGoal::_forEachKin(ServerNetworkHandler &owner, MobActor &mob, bool includeSelf, Visitor &&visit) const {
    const float range = mSettings.mBroadcastRange;
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(range, range, range);
    const float reach = std::sqrt(3.0f * range * range) + mob.getSize().mHeight;

    owner.forEachActorNear(mob.getPosition(), reach, [&](ServerActor &actor) {
        MobActor *kin = dynamic_cast<MobActor *>(&actor);
        if (kin == nullptr || (kin == &mob && !includeSelf) || !kin->isAlive() || !kin->sharesLevelWith(mob)
            || std::strcmp(kin->getIdentifier(), mob.getIdentifier()) != 0
            || !ActorPushSystem::boundingBoxOf(*kin).intersectsWith(area))
            return;

        visit(*kin);
    });
}
