#include "Actor/AI/Goal/AdmireItemGoal.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Mob/MobActor.h"

#include <algorithm>
#include <random>
#include <utility>

namespace {
    std::mt19937 &admireRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

AdmireItemGoal::AdmireItemGoal(std::string sound, int32_t minSoundInterval, int32_t maxSoundInterval,
                               std::shared_ptr<json::Value> onStart, std::shared_ptr<json::Value> onStop)
        : mSound(std::move(sound)), mMinSoundInterval(std::max(1, minSoundInterval)),
          mMaxSoundInterval(std::max(std::max(1, minSoundInterval), maxSoundInterval)), mOnStart(std::move(onStart)),
          mOnStop(std::move(onStop)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

int32_t AdmireItemGoal::_nextSound() const {
    return std::uniform_int_distribution<int32_t>(mMinSoundInterval, mMaxSoundInterval)(admireRandom());
}

bool AdmireItemGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mob.getAdmiration().isAdmiring();
}

void AdmireItemGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getNavigation().stop(mob);
    mSoundTicks = _nextSound();
    EntityEvents::fireTrigger(owner, mob, mOnStart.get());
}

void AdmireItemGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    EntityEvents::fireTrigger(owner, mob, mOnStop.get());
}

void AdmireItemGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mSound.empty() || --mSoundTicks > 0)
        return;

    mSoundTicks = _nextSound();
    mob.playDefinitionSound(owner, mSound);
}
