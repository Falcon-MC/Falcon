#include "World/Actor/AI/Goal/OfferFlowerGoal.h"

#include "Core/Math/AxisAlignedBB.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <random>
#include <utility>

namespace {
    const char *const TAKE_FLOWER_BEHAVIOR = "minecraft:behavior.take_flower";
    // the actor events the client shows and hides the golem's flower with
    const EntityEventType OFFER_FLOWER_EVENT = (EntityEventType) 19;
    const EntityEventType WITHDRAW_FLOWER_EVENT = (EntityEventType) 20;

    std::mt19937 &flowerRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

OfferFlowerGoal::OfferFlowerGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Look);
}

bool OfferFlowerGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(flowerRandom()) > mSettings.mChanceToStart)
        return false;
    if (mSettings.mFilters != nullptr && !EntityFilter::test(*mSettings.mFilters, owner, mob))
        return false;

    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(
            mSettings.mSearchArea.x, mSettings.mSearchArea.y, mSettings.mSearchArea.z);
    const float reach = std::max(area.mMaxX - area.mMinX, std::max(area.mMaxY - area.mMinY, area.mMaxZ - area.mMinZ));
    int64_t found = 0;
    owner.forEachActorNear(mob.getPosition(), reach, [&](ServerActor &actor) {
        if (found != 0 || &actor == &mob || !actor.isAlive() || !actor.sharesLevelWith(mob))
            return;
        const MobActor *candidate = dynamic_cast<const MobActor *>(&actor);
        if (candidate == nullptr || candidate->getComponent(TAKE_FLOWER_BEHAVIOR) == nullptr
            || !ActorPushSystem::boundingBoxOf(*candidate).intersectsWith(area))
            return;
        found = candidate->getUniqueId();
    });
    mTargetId = found;
    return found != 0;
}

bool OfferFlowerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) mob;
    const ServerActor *target = owner.getActor(mTargetId);
    return target != nullptr && target->isAlive() && owner.getCurrentTick() < mEndTick;
}

void OfferFlowerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mEndTick = owner.getCurrentTick() + mSettings.mDurationTicks;
    owner.broadcastActorEvent(mob, OFFER_FLOWER_EVENT);
}

void OfferFlowerGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    owner.broadcastActorEvent(mob, WITHDRAW_FLOWER_EVENT);
    mob.getLookControl().clear();
    mTargetId = 0;
}

void OfferFlowerGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (const ServerActor *target = owner.getActor(mTargetId))
        mob.getLookControl().setLookAt(target->getPosition());
}
