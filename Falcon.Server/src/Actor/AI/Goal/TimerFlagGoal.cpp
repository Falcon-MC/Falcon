#include "Actor/AI/Goal/TimerFlagGoal.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <random>
#include <utility>

namespace {
    std::mt19937 &timerRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

TimerFlagGoal::TimerFlagGoal(ActorFlag flag, int32_t minDuration, int32_t maxDuration, int32_t minCooldown,
                             int32_t maxCooldown, std::shared_ptr<json::Value> onStart,
                             std::shared_ptr<json::Value> onEnd, uint8_t controlFlags)
        : mFlag(flag), mMinDuration(minDuration), mMaxDuration(maxDuration), mMinCooldown(minCooldown),
          mMaxCooldown(maxCooldown), mOnStart(std::move(onStart)), mOnEnd(std::move(onEnd)) {
    setRequiredControlFlags(controlFlags);
}

int32_t TimerFlagGoal::_roll(int32_t minimum, int32_t maximum) {
    if (maximum <= minimum)
        return minimum;

    return std::uniform_int_distribution<int32_t>(minimum, maximum)(timerRandom());
}

void TimerFlagGoal::_setFlag(ServerNetworkHandler &owner, MobActor &mob, bool value) const {
    if (mob.getFlags().get(mFlag) == value)
        return;

    mob.getFlags().set(mFlag, value);
    owner.syncActorFlags(mob);
}

bool TimerFlagGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;

    return owner.getCurrentTick() >= mCooldownEndTick;
}

bool TimerFlagGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;

    return owner.getCurrentTick() < mEndTick;
}

void TimerFlagGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mEndTick = owner.getCurrentTick() + _roll(mMinDuration, mMaxDuration);
    _setFlag(owner, mob, true);
    EntityEvents::fireTrigger(owner, mob, mOnStart.get());
}

void TimerFlagGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mCooldownEndTick = owner.getCurrentTick() + std::max(1, _roll(mMinCooldown, mMaxCooldown));
    _setFlag(owner, mob, false);
    EntityEvents::fireTrigger(owner, mob, mOnEnd.get());
}
