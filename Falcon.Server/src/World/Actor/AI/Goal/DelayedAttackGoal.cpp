#include "World/Actor/AI/Goal/DelayedAttackGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"

#include <algorithm>

namespace {
    const char *const SWING_SOUND = "attack.strong";
    const float MIN_REACH_WIDTH = 0.5f;

    float widthOf(const Actor &actor) {
        const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(actor);
        return box.mMaxX - box.mMinX;
    }
}

DelayedAttackGoal::DelayedAttackGoal(float speed, float maxRange, float reachMultiplier, int32_t attackDuration,
                                     int32_t hitDelay)
        : MeleeAttackGoal(speed, maxRange, 0, 0.0f), mReachMultiplier(reachMultiplier),
          mAttackDuration(attackDuration), mHitDelay(hitDelay), mSwingTicks(attackDuration),
          mTicksSinceSwing(attackDuration) {}

bool DelayedAttackGoal::_swinging(const MobActor &mob) const {
    return mob.getFlags().get(ActorFlag::DelayedAttacking);
}

void DelayedAttackGoal::_setSwinging(ServerNetworkHandler &owner, MobActor &mob, bool swinging) {
    if (_swinging(mob) == swinging)
        return;
    mob.getFlags().set(ActorFlag::DelayedAttacking, swinging);
    owner.syncActorFlags(mob);
}

bool DelayedAttackGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    // a swing under way always finishes
    if (_swinging(mob) && mSwingTicks > 0 && mSwingTicks < mAttackDuration)
        return true;
    return MeleeAttackGoal::canContinueToUse(owner, mob);
}

void DelayedAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    _setSwinging(owner, mob, false);
    mSwingTicks = mAttackDuration;
    mTicksSinceSwing = mAttackDuration;
    MeleeAttackGoal::stop(owner, mob);
}

void DelayedAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getTarget(owner) == nullptr) {
        if (_swinging(mob) && mSwingTicks > 0 && mSwingTicks < mAttackDuration)
            --mSwingTicks;
        return;
    }
    MeleeAttackGoal::tick(owner, mob);
}

void DelayedAttackGoal::_tryAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &target) {
    // the game adds the target's width to a squared reach, unsquared
    const float width = std::max(MIN_REACH_WIDTH, widthOf(mob));
    const float reach = mReachMultiplier * width;
    const bool inReach = reach * reach + widthOf(target) >= mob.distanceSquaredTo(target);

    if (mTicksSinceSwing++ < mAttackDuration || !target.isAlive())
        return;

    const bool canHit = inReach && mob.canSee(owner, target);
    if (canHit && !_swinging(mob) && mSwingTicks == mAttackDuration) {
        owner.playLevelSound(owner.getLevelFor(mob), SWING_SOUND, mob.getPosition());
        _setSwinging(owner, mob, true);
    }
    if (!_swinging(mob))
        return;

    if (mSwingTicks == mAttackDuration - mHitDelay && canHit)
        _attack(owner, mob, target);
    if (mSwingTicks <= 0) {
        mSwingTicks = mAttackDuration;
        _setSwinging(owner, mob, false);
        mTicksSinceSwing = 0;
        return;
    }
    --mSwingTicks;
}
