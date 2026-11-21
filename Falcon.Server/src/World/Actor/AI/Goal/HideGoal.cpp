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

// a bell comes before the raid
bool HideGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getComponent(HIDE_COMPONENT) == nullptr)
        return false;
    if (mob.heardBell()) {
        mRaidHiding = false;
        return MoveToPoiGoal::canUse(owner, mob);
    }
    mRaidHiding = isVillageRaided(mob);
    return mRaidHiding && MoveToPoiGoal::canUse(owner, mob);
}

bool HideGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!mRaidHiding)
        return MoveToPoiGoal::canContinueToUse(owner, mob);
    if (!isVillageRaided(mob) && mHiddenTicks >= mDurationTicks)
        return false;
    // once there the walk's stay limit no longer applies
    return isReached() || MoveToPoiGoal::canContinueToUse(owner, mob);
}

void HideGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::start(owner, mob);
    mHiddenTicks = 0;
}

// the bell calls it back until one stay outlasts the duration, or four more walks went by
void HideGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::stop(owner, mob);
    if (mRaidHiding) {
        mob.setHeardBell(false);
        mBellTries = 0;
    } else if (mob.heardBell()) {
        if (mHiddenTicks > mDurationTicks || mBellTries >= BELL_TRIES) {
            mob.setHeardBell(false);
            mBellTries = 0;
        } else {
            ++mBellTries;
        }
    } else if (mBellTries >= BELL_TRIES) {
        mBellTries = 0;
    }
    mRaidHiding = false;
    mHiddenTicks = 0;
}

void HideGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::tick(owner, mob);
    if (isReached())
        ++mHiddenTicks;
}
