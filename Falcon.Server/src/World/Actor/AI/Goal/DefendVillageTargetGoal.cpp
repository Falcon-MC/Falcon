#include "World/Actor/AI/Goal/DefendVillageTargetGoal.h"

#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Village/Village.h"
#include "Network/ServerNetworkHandler.h"

#include <random>
#include <utility>

namespace {
    std::mt19937 &defendRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

DefendVillageTargetGoal::DefendVillageTargetGoal(float attackChance, float range,
                                                 std::vector<std::shared_ptr<json::Value>> filters)
        : mAttackChance(attackChance), mRangeSquared(range * range), mFilters(std::move(filters)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Target);
}

Actor *DefendVillageTargetGoal::pickAggressor(ServerNetworkHandler &owner, const MobActor &mob) {
    const MobDweller &dweller = mob.getDweller();
    const std::shared_ptr<Village> village = dweller.isActive() ? dweller.getVillage() : nullptr;
    if (village == nullptr)
        return nullptr;

    // a player aggressor stands for every player the village holds hostile
    Actor *aggressor = village->findClosestAggressor(owner, mob);
    if (aggressor != nullptr && aggressor->isPlayer())
        aggressor = village->findClosestHostilePlayer(owner, mob);
    return aggressor;
}

bool DefendVillageTargetGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    mFoundRuntimeId = 0;
    Actor *aggressor = pickAggressor(owner, mob);
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(defendRandom()) >= mAttackChance)
        return false;
    if (aggressor == nullptr || !aggressor->isAlive() || !mob.canTarget(*aggressor))
        return false;

    for (const std::shared_ptr<json::Value> &filters: mFilters) {
        if (filters != nullptr && !EntityFilter::test(*filters, owner, mob, aggressor))
            return false;
    }

    mFoundRuntimeId = aggressor->getRuntimeId();
    return true;
}

bool DefendVillageTargetGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *target = mob.getTarget(owner);
    return target != nullptr && target->isAlive() && mob.canTarget(*target)
           && mob.distanceSquaredTo(*target) <= mRangeSquared;
}

void DefendVillageTargetGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    if (mFoundRuntimeId != 0)
        mob.setTarget(owner, mFoundRuntimeId);
    mFoundRuntimeId = 0;
}

void DefendVillageTargetGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.clearTarget();
}
