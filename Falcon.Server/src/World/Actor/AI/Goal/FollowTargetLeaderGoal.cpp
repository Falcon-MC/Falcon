#include "World/Actor/AI/Goal/FollowTargetLeaderGoal.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/RideSystem.h"
#include "World/Level/Level.h"

#include <cmath>
#include <utility>

namespace {
    const float SEARCH_HORIZONTAL = 16.0f;
    const float SEARCH_VERTICAL = 12.0f;
    // without always_look_for_leader a mob gives up after this many searches
    const int32_t SEARCH_LIMIT = 5;
    const float STEP_SHARE_OF_FOLLOW_RANGE = 0.75f;
}

FollowTargetLeaderGoal::FollowTargetLeaderGoal(Settings settings) : mSettings(std::move(settings)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool FollowTargetLeaderGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    Actor *leader = _leader(owner, mob);
    const bool searching = mSettings.mAlwaysLookForLeader || mSearches < SEARCH_LIMIT;
    if (searching && (leader == nullptr || !leader->isAlive()) && owner.getCurrentTick() >= mNextSearchTick) {
        leader = _search(owner, mob);
        if (leader != nullptr)
            mob.setTargetCaptainId(leader->getUniqueId());
        ++mSearches;
        mNextSearchTick = owner.getCurrentTick() + mSettings.mSearchCooldownTicks;
    }

    if (leader == nullptr || !leader->isAlive())
        return false;

    if (mSettings.mAlwaysLookForLeader && !_isLeader(owner, mob, *leader)) {
        mob.setTargetCaptainId(0);
        return false;
    }

    const float distance = mob.distanceSquaredTo(*leader);
    return distance >= mSettings.mFollowDistance * mSettings.mFollowDistance
           && distance <= mSettings.mWithinRadius * mSettings.mWithinRadius;
}

bool FollowTargetLeaderGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *leader = _leader(owner, mob);
    if (leader == nullptr || !leader->isAlive())
        return false;

    const float distance = mob.distanceSquaredTo(*leader);
    return distance > mSettings.mFollowDistance * mSettings.mFollowDistance
           && distance < mSettings.mWithinRadius * mSettings.mWithinRadius;
}

void FollowTargetLeaderGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    _moveTowards(owner, mob);
}

void FollowTargetLeaderGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getNavigation().isDone())
        _moveTowards(owner, mob);
}

Actor *FollowTargetLeaderGoal::_leader(ServerNetworkHandler &owner, const MobActor &mob) const {
    const int64_t leaderId = mob.getTargetCaptainId();
    return leaderId == 0 ? nullptr : RideSystem::resolve(owner, leaderId);
}

Actor *FollowTargetLeaderGoal::_search(ServerNetworkHandler &owner, MobActor &mob) const {
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(mob).expand(SEARCH_HORIZONTAL, SEARCH_VERTICAL,
                                                                          SEARCH_HORIZONTAL);
    const float reach = std::sqrt(2.0f * SEARCH_HORIZONTAL * SEARCH_HORIZONTAL + SEARCH_VERTICAL * SEARCH_VERTICAL)
                        + mob.getSize().mHeight;

    Actor *found = nullptr;
    owner.forEachActorNear(mob.getPosition(), reach, [&](ServerActor &actor) {
        if (found != nullptr || &actor == &mob || !actor.isAlive() || !actor.sharesLevelWith(mob)
            || !ActorPushSystem::boundingBoxOf(actor).intersectsWith(area) || !_isLeader(owner, mob, actor))
            return;

        found = &actor;
    });
    return found;
}

bool FollowTargetLeaderGoal::_isLeader(ServerNetworkHandler &owner, const MobActor &mob,
                                       const Actor &candidate) const {
    return mSettings.mLeaderFilters != nullptr && EntityFilter::test(*mSettings.mLeaderFilters, owner, mob, &candidate);
}

bool FollowTargetLeaderGoal::_inBand(const MobActor &mob, const Actor &leader) const {
    const float distance = mob.distanceSquaredTo(leader);
    return distance <= mSettings.mWithinRadius * mSettings.mWithinRadius
           && distance >= mSettings.mFollowDistance * mSettings.mFollowDistance;
}

// a step toward the leader of three quarters of the follow range, dropped on the top solid block there
void FollowTargetLeaderGoal::_moveTowards(ServerNetworkHandler &owner, MobActor &mob) {
    const Actor *leader = _leader(owner, mob);
    if (leader == nullptr || !leader->isAlive() || !_inBand(mob, *leader))
        return;

    const Vector3f position = mob.getPosition();
    const Vector3f leaderPosition = leader->getPosition();
    const float dx = leaderPosition.x - position.x;
    const float dy = leaderPosition.y - position.y;
    const float dz = leaderPosition.z - position.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001f)
        return;

    const float step = mob.getFollowRange() * STEP_SHARE_OF_FOLLOW_RANGE;
    const float x = position.x + std::floor(dx / length * step);
    const float y = position.y + std::floor(dy / length * step);
    const float z = position.z + std::floor(dz / length * step);
    const int32_t top = owner.getLevelFor(mob).getAboveTopSolidBlock((int32_t) std::floor(x), (int32_t) std::floor(y),
                                                                     (int32_t) std::floor(z));
    mob.getNavigation().moveTo(Vector3f(x, (float) top, z), mSettings.mSpeed);
}
