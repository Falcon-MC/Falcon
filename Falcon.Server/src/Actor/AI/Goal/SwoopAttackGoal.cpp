#include "Actor/AI/Goal/SwoopAttackGoal.h"

#include "Actor/AI/Goal/MobAttack.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/Movement/ActorPushSystem.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <random>

namespace {
    const char *const SWOOP_SOUND = "swoop";

    std::mt19937 &swoopRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

SwoopAttackGoal::SwoopAttackGoal(float speed, float damageReach, int32_t minDelayTicks, int32_t maxDelayTicks)
        : mSpeed(speed), mDamageReach(damageReach), mMinDelayTicks(minDelayTicks),
          mMaxDelayTicks(std::max(minDelayTicks, maxDelayTicks)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool SwoopAttackGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    if (target == nullptr || !target->isAlive())
        return false;

    if (mNextSwoopTick < 0)
        _scheduleNext(owner);

    return owner.getCurrentTick() >= mNextSwoopTick;
}

bool SwoopAttackGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return !mDone && target != nullptr && target->isAlive() && mob.getHurtCount() == mHurtCount;
}

void SwoopAttackGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mDone = false;
    mHurtCount = mob.getHurtCount();
    mob.getLookControl().setPitchEnabled(true);
    owner.playLevelSound(owner.getLevelFor(mob), SWOOP_SOUND, mob.getPosition(), mob.getIdentifier());
}

void SwoopAttackGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    _scheduleNext(owner);
    mob.getMoveControl().stop();
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
}

void SwoopAttackGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *target = mob.getTarget(owner);
    if (target == nullptr)
        return;

    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(*target);
    const Vector3f aim((box.mMinX + box.mMaxX) * 0.5f, (box.mMinY + box.mMaxY) * 0.5f, (box.mMinZ + box.mMaxZ) * 0.5f);
    mob.getMoveControl().setWantedPosition(aim, mSpeed);
    mob.getLookControl().setLookAt(aim);

    if (!MobAttack::isTouching(mob, *target, mDamageReach))
        return;

    MobAttack::hit(owner, mob, *target, mob.getAttackDamage(owner.getProperties().getDifficulty()));
    mDone = true;
}

void SwoopAttackGoal::_scheduleNext(ServerNetworkHandler &owner) {
    mNextSwoopTick = owner.getCurrentTick()
                     + std::uniform_int_distribution<int32_t>(mMinDelayTicks, mMaxDelayTicks)(swoopRandom());
}
