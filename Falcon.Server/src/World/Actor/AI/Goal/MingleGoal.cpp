#include "World/Actor/AI/Goal/MingleGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "Network/ServerNetworkHandler.h"

#include <cfloat>
#include <utility>

namespace {
    const int32_t REPATH_INTERVAL = 10;
    // the game looks for a partner every 10 ticks once at the meeting point
    const int32_t SEARCH_INTERVAL = 10;
    const float SEARCH_RANGE = 8.0f;
}

MingleGoal::MingleGoal(Settings settings) : MoveToPoiGoal(settings.mSpeed, PoiType::MeetingArea),
                                            mSettings(std::move(settings)) {}

bool MingleGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCooldown > 0) {
        --mCooldown;
        return false;
    }
    return MoveToPoiGoal::canUse(owner, mob);
}

bool MingleGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mTicksLeft <= 0)
        return false;
    return isReached() || MoveToPoiGoal::canContinueToUse(owner, mob);
}

void MingleGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::start(owner, mob);
    mTicksLeft = mSettings.mDurationTicks;
    mSearchTicks = 0;
    mRepathTicks = 0;
    mPartnerId = 0;
}

void MingleGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::stop(owner, mob);
    mCooldown = mSettings.mCooldownTicks;
    mPartnerId = 0;
}

void MingleGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mPartnerId == 0) {
        MoveToPoiGoal::tick(owner, mob);
        if (!isReached())
            return;
        // the time at the meeting point runs out, partner or not
        --mTicksLeft;
        if (mSearchTicks++ % SEARCH_INTERVAL != 0)
            return;

        float closest = FLT_MAX;
        owner.forEachActorNear(mob.getPosition(), SEARCH_RANGE, [&](ServerActor &actor) {
            MobActor *candidate = dynamic_cast<MobActor *>(&actor);
            if (candidate == nullptr || candidate == &mob || !candidate->isAlive() || !candidate->sharesLevelWith(mob)
                || candidate->getIdentifier() != mSettings.mPartnerType || candidate->getFlags().get(ActorFlag::Baby))
                return;
            const float distance = mob.distanceSquaredTo(*candidate);
            if (distance < closest && distance <= SEARCH_RANGE * SEARCH_RANGE) {
                closest = distance;
                mPartnerId = candidate->getUniqueId();
            }
        });
        return;
    }

    const Actor *partner = owner.getActor(mPartnerId);
    if (partner == nullptr || !partner->isAlive() || !partner->sharesLevelWith(mob)) {
        mPartnerId = 0;
        return;
    }

    --mTicksLeft;
    mob.getLookControl().setLookAt(partner->getPosition());
    if (mob.distanceSquaredTo(*partner) <= mSettings.mMingleDistance * mSettings.mMingleDistance) {
        mob.getNavigation().stop(mob);
        mRepathTicks = 0;
        return;
    }
    // moveTo always asks for a new search: the path budget is shared by every mob
    if (--mRepathTicks > 0)
        return;
    mRepathTicks = REPATH_INTERVAL;
    mob.getNavigation().moveTo(partner->getPosition(), mSettings.mSpeed);
}
