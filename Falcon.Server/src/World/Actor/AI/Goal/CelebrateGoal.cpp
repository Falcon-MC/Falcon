#include "World/Actor/AI/Goal/CelebrateGoal.h"

#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/MobActor.h"
#include "Network/ServerNetworkHandler.h"

#include <random>
#include <utility>

namespace {
    const float TICKS_PER_SECOND = 20.0f;
    // the jump control asks for a full jump, which takes the mob's own jump power
    const float FULL_JUMP_HEIGHT = 1.0f;

    std::mt19937 &celebrateRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

CelebrateGoal::CelebrateGoal(Settings settings) : mSettings(std::move(settings)) {
    // control flags 5, as the definition builds the goal (sub_147685A50)
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Jump);
}

bool CelebrateGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    // a mob with a target it can still find does not celebrate
    return mob.getTarget(owner) == nullptr;
}

bool CelebrateGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    // sub_14D2FF940: the elapsed time is never reset, the goal runs once for the life of the component group
    (void) owner;
    (void) mob;
    return mElapsedTicks < (int32_t) (mSettings.mDuration * TICKS_PER_SECOND);
}

void CelebrateGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    // sub_14D2FF140: the sound interval is drawn before the jump interval
    mSoundTicks = _rollTicks(mSettings.mSoundIntervalMin, mSettings.mSoundIntervalMax);
    mJumpTicks = _rollTicks(mSettings.mJumpIntervalMin, mSettings.mJumpIntervalMax);
    _setCelebrating(owner, mob, true);
}

void CelebrateGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    _setCelebrating(owner, mob, false);
    EntityEvents::fireTrigger(owner, mob, mSettings.mOnEnd.get());
}

void CelebrateGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    // both timers count down every tick, the sound one is tested before its decrement
    const int32_t soundTicks = mSoundTicks;
    --mJumpTicks;
    --mSoundTicks;
    ++mElapsedTicks;

    if (soundTicks <= 1) {
        mob.playDefinitionSound(owner, mSettings.mSound);
        mSoundTicks = _rollTicks(mSettings.mSoundIntervalMin, mSettings.mSoundIntervalMax);
    }

    if (mJumpTicks > 0)
        return;

    if (!mob.isRiding())
        mob.getJumpControl().jump(FULL_JUMP_HEIGHT);
    mJumpTicks = _rollTicks(mSettings.mJumpIntervalMin, mSettings.mJumpIntervalMax);
}

int32_t CelebrateGoal::_rollTicks(float minimum, float maximum) {
    const float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(celebrateRandom());
    return (int32_t) (((maximum - minimum) * roll + minimum) * TICKS_PER_SECOND);
}

void CelebrateGoal::_setCelebrating(ServerNetworkHandler &owner, MobActor &mob, bool value) {
    if (mob.getFlags().get(ActorFlag::Celebrating) == value)
        return;

    mob.getFlags().set(ActorFlag::Celebrating, value);
    owner.syncActorFlags(mob);
}
