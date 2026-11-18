#include "World/Actor/AI/Goal/TradeWithPlayerGoal.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <utility>

TradeWithPlayerGoal::TradeWithPlayerGoal(std::shared_ptr<json::Value> filters, float maxDistance)
        : mFilters(std::move(filters)), mMaxDistanceSquared(maxDistance * maxDistance) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Jump);
}

bool TradeWithPlayerGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getEconomy().findTradingPlayer(owner) == nullptr)
        return false;
    return mFilters == nullptr || EntityFilter::test(*mFilters, owner, mob);
}

bool TradeWithPlayerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!canUse(owner, mob))
        return false;
    const ServerPlayer *player = mob.getEconomy().findTradingPlayer(owner);
    return mob.distanceSquaredTo(*player) <= mMaxDistanceSquared;
}

void TradeWithPlayerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}
