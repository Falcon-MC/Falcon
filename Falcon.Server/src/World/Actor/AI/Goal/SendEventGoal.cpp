#include "World/Actor/AI/Goal/SendEventGoal.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/SpellCasting.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"

#include <utility>

namespace {
    struct Candidate {
        int32_t mChoice;
        int64_t mReceiver;
    };
}

SendEventGoal::SendEventGoal(std::vector<Choice> choices) : mChoices(std::move(choices)) {}

MobActor *SendEventGoal::_receiver(ServerNetworkHandler &owner) const {
    if (mReceiver == -1)
        return nullptr;
    MobActor *receiver = dynamic_cast<MobActor *>(owner.getEntity(mReceiver));
    return receiver != nullptr && receiver->isAlive() ? receiver : nullptr;
}

bool SendEventGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    std::vector<Candidate> candidates;
    int32_t totalWeight = 0;
    for (int32_t index = 0; index < (int32_t) mChoices.size(); ++index) {
        const Choice &choice = mChoices[index];
        const float range = choice.mMaxRange;
        const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(range, range, range);
        owner.forEachActorNear(mob.getPosition(), range + mob.getSize().mWidth, [&](ServerActor &actor) {
            if (!actor.sharesLevelWith(mob) || !area.intersectsWith(ActorPushSystem::boundingBoxOf(actor)))
                return;
            const float distance = mob.distanceSquaredTo(actor);
            if (distance > choice.mMaxRange * choice.mMaxRange || distance < choice.mMinRange * choice.mMinRange)
                return;
            if (choice.mFilters != nullptr && !EntityFilter::test(*choice.mFilters, owner, mob, &actor))
                return;
            candidates.push_back({index, actor.getUniqueId()});
            totalWeight = (int32_t) ((float) totalWeight + choice.mWeight);
        });
    }

    mChoice = -1;
    if (candidates.empty() || totalWeight <= 0)
        return false;

    // weights are walked as floats against an integer roll, as the game does
    int32_t roll = std::uniform_int_distribution<int32_t>(0, totalWeight - 1)(mRandom);
    for (const Candidate &candidate: candidates) {
        const float weight = mChoices[candidate.mChoice].mWeight;
        if (weight > (float) roll) {
            mChoice = candidate.mChoice;
            mReceiver = candidate.mReceiver;
            break;
        }
        roll = (int32_t) ((float) roll - weight);
    }
    if (mChoice < 0 || mChoices[mChoice].mSequence.empty())
        return false;
    return mCooldownEndTick <= owner.getCurrentTick();
}

bool SendEventGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;
    if (_receiver(owner) == nullptr || mChoice < 0)
        return false;
    if (mCastTicks > 0)
        return true;
    return mStep < (int32_t) mChoices[mChoice].mSequence.size();
}

void SendEventGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mStep = 0;
    mDelayTicks = -1;
    if (mChoice < 0)
        return;

    const Choice &choice = mChoices[mChoice];
    Level &level = owner.getLevelFor(mob);
    if (!choice.mStartSoundEvent.empty())
        owner.playLevelSound(level, choice.mStartSoundEvent, mob.getPosition());
    mCastTicks = choice.mCastDuration;
    SpellCasting::sendColor(owner, mob, choice.mParticleColor);
    if (choice.mDoCasting)
        SpellCasting::setCasting(owner, mob, mCastTicks > 0);
    if (!choice.mSequence.empty() && !choice.mSequence.front().mSoundEvent.empty())
        owner.playLevelSound(level, choice.mSequence.front().mSoundEvent, mob.getPosition());
}

void SendEventGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    SpellCasting::setCasting(owner, mob, false);
    if (mChoice >= 0)
        mCooldownEndTick = owner.getCurrentTick() + mChoices[mChoice].mCooldown;
}

void SendEventGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    MobActor *receiver = _receiver(owner);
    if (receiver == nullptr || mChoice < 0)
        return;

    const Choice &choice = mChoices[mChoice];
    if (choice.mLookAtTarget)
        mob.getLookControl().setLookAt(receiver->getPosition());
    if (mCastTicks-- == 1)
        SpellCasting::setCasting(owner, mob, false);

    if (mStep >= (int32_t) choice.mSequence.size())
        return;
    ++mDelayTicks;
    // a step's delay is taken from the count, not reset: a long wait can run several steps in one tick
    while (mStep < (int32_t) choice.mSequence.size() && mDelayTicks >= choice.mSequence[mStep].mBaseDelay) {
        const Step &step = choice.mSequence[mStep];
        receiver->fireEvent(owner, step.mEvent);
        mDelayTicks -= step.mBaseDelay;
        if (++mStep >= (int32_t) choice.mSequence.size())
            break;
        const std::string &sound = choice.mSequence[mStep].mSoundEvent;
        if (!sound.empty())
            owner.playLevelSound(owner.getLevelFor(mob), sound, mob.getPosition());
    }
}
