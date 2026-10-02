#include "Actor/AI/Goal/PanicGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

PanicGoal::PanicGoal(float speed, int32_t range, int32_t interval, int32_t duration, bool avoidWater,
                     int32_t maxRetries)
        : RandomStrollGoal(speed, range, interval, avoidWater, maxRetries), mDuration(duration) {
}

bool PanicGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return owner.getCurrentTick() - mob.getLastHurtTick() <= mDuration;
}

void PanicGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    RandomStrollGoal::start(owner, mob);
    mob.setPanicking(true);
}

void PanicGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    RandomStrollGoal::stop(owner, mob);
    mob.setPanicking(false);
}

bool PanicGoal::shouldPickTarget(MobActor &mob) const {
    return getTicksSinceTarget() >= getInterval() || mob.getNavigation().isDone();
}
