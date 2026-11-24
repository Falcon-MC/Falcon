#include "World/Actor/AI/Goal/CelebrateSurviveGoal.h"

#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Item/FireworkRocketItem.h"
#include "Network/ServerNetworkHandler.h"

#include <utility>

CelebrateSurviveGoal::CelebrateSurviveGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Jump);
}

bool CelebrateSurviveGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return true;
}

bool CelebrateSurviveGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mTicksLeft > 0;
}

void CelebrateSurviveGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTicksLeft = mSettings.mDurationTicks;
    mNextFirework = MobActor::randomRange(mSettings.mFireworksMinTicks, mSettings.mFireworksMaxTicks);
}

void CelebrateSurviveGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    // the end event removes the group holding this goal: only a celebration that ran out fires it
    if (mTicksLeft <= 0)
        EntityEvents::fireTrigger(owner, mob, mSettings.mOnEnd.get());
}

void CelebrateSurviveGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    --mTicksLeft;
    if (--mNextFirework > 0)
        return;

    mNextFirework = MobActor::randomRange(mSettings.mFireworksMinTicks, mSettings.mFireworksMaxTicks);
    const Vector3f &position = mob.getPosition();
    FireworkRocketItem::launch(owner, owner.getLevelFor(mob),
                               Vector3f(position.x, position.y + mob.getSize().mHeight, position.z),
                               ItemStack::air());
}
