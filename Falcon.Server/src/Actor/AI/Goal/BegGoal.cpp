#include "Actor/AI/Goal/BegGoal.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <random>
#include <utility>

namespace {
    std::mt19937 &begRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

BegGoal::BegGoal(BehaviorItems items, float lookDistance, int32_t minLookTicks, int32_t maxLookTicks)
        : mItems(std::move(items)), mLookDistance(lookDistance), mMinLookTicks(minLookTicks),
          mMaxLookTicks(std::max(minLookTicks, maxLookTicks)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool BegGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return !mItems.isEmpty() && mItems.findNearestHolder(owner, mob, mLookDistance) != nullptr;
}

bool BegGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mLookTicks > 0 && mItems.findNearestHolder(owner, mob, mLookDistance) != nullptr;
}

void BegGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mLookTicks = std::uniform_int_distribution<int32_t>(mMinLookTicks, mMaxLookTicks)(begRandom());
    mob.getLookControl().setPitchEnabled(true);
    _setInterested(owner, mob, true);
}

void BegGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getLookControl().clear();
    mob.getLookControl().setPitchEnabled(false);
    _setInterested(owner, mob, false);
}

void BegGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *player = mItems.findNearestHolder(owner, mob, mLookDistance);
    if (player == nullptr)
        return;

    mob.getLookControl().setLookAt(player->getPosition());
    mLookTicks--;
}

void BegGoal::_setInterested(ServerNetworkHandler &owner, MobActor &mob, bool interested) {
    if (mob.getFlags().get(ActorFlag::Interested) == interested)
        return;

    mob.getFlags().set(ActorFlag::Interested, interested);
    owner.syncActorFlags(mob);
}
