#include "World/Actor/AI/Goal/HideGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Village/Village.h"

#include <memory>

namespace {
    // the hide component marks a mob that may hide; the goal only runs with it
    const char *const HIDE_COMPONENT = "minecraft:hide";
}

HideGoal::HideGoal(float speed, PoiType type, int32_t durationTicks)
        : MoveToPoiGoal(speed, type), mDurationTicks(durationTicks) {}

bool HideGoal::isVillageRaided(const MobActor &mob) {
    const MobDweller &dweller = mob.getDweller();
    const std::shared_ptr<Village> village = dweller.isActive() ? dweller.getVillage() : nullptr;
    return village != nullptr && village->getRaid() != nullptr && !village->isRaidFinished();
}

bool HideGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mob.getComponent(HIDE_COMPONENT) != nullptr && isVillageRaided(mob) && MoveToPoiGoal::canUse(owner, mob);
}

bool HideGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!isVillageRaided(mob) && mHiddenTicks >= mDurationTicks)
        return false;
    // once there the walk's stay limit no longer applies
    return isReached() || MoveToPoiGoal::canContinueToUse(owner, mob);
}

void HideGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::start(owner, mob);
    mHiddenTicks = 0;
}

void HideGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::tick(owner, mob);
    if (isReached())
        ++mHiddenTicks;
}
