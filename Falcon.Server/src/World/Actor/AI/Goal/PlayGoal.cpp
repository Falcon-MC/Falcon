#include "World/Actor/AI/Goal/PlayGoal.h"

#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "Network/ServerNetworkHandler.h"

#include <cfloat>
#include <cmath>
#include <random>
#include <utility>

namespace {
    std::mt19937 &playRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

PlayGoal::PlayGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

int64_t PlayGoal::_findFriend(ServerNetworkHandler &owner, MobActor &mob) const {
    const Vector3f &area = mSettings.mFriendSearchArea;
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob).expand(area.x, area.y, area.z);
    int64_t friendId = 0;
    float closest = FLT_MAX;
    owner.forEachActorNear(mob.getPosition(), std::sqrt(area.x * area.x + area.y * area.y + area.z * area.z),
                           [&](ServerActor &actor) {
                               if (&actor == &mob || !actor.isAlive() || !actor.sharesLevelWith(mob)
                                   || !ActorPushSystem::boundingBoxOf(actor).intersectsWith(box))
                                   return;
                               bool matches = mSettings.mFriendFilters.empty();
                               for (const CompiledEntityFilter &filters: mSettings.mFriendFilters)
                                   matches = matches || filters == nullptr
                                             || EntityFilter::test(*filters, owner, mob, &actor);
                               const float distance = mob.distanceSquaredTo(actor);
                               if (matches && distance < closest) {
                                   closest = distance;
                                   friendId = actor.getUniqueId();
                               }
                           });
    return friendId;
}

bool PlayGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (!mob.getFlags().get(ActorFlag::Baby))
        return false;
    return mSettings.mChanceToStart <= 0.0f
           || std::uniform_real_distribution<float>(0.0f, 1.0f)(playRandom()) < mSettings.mChanceToStart;
}

bool PlayGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    return mTicksLeft > 0 && mob.getFlags().get(ActorFlag::Baby);
}

void PlayGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mTicksLeft = mSettings.mMaxPlayTicks;
    mFriendId = _findFriend(owner, mob);
}

void PlayGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mFriendId = 0;
    mob.getNavigation().stop(mob);
}

void PlayGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    --mTicksLeft;
    const Actor *friendActor = mFriendId == 0 ? nullptr : owner.getActor(mFriendId);
    if (friendActor != nullptr && friendActor->isAlive() && friendActor->sharesLevelWith(mob)) {
        if (mob.distanceSquaredTo(*friendActor) > mSettings.mFollowDistance * mSettings.mFollowDistance
            && mob.getNavigation().isDone())
            mob.getNavigation().moveTo(friendActor->getPosition(), mSettings.mSpeed);
        return;
    }

    mFriendId = 0;
    if (!mob.getNavigation().isDone())
        return;
    Vector3i ground;
    if (RandomGroundPosition::find(owner.getLevelFor(mob), mob, mSettings.mRandomRange, mSettings.mRandomHeight,
                                   nullptr, playRandom(), ground))
        mob.getNavigation().moveTo(Vector3f((float) ground.x + 0.5f, (float) ground.y, (float) ground.z + 0.5f),
                                   mSettings.mSpeed);
}
