#include "World/Actor/AI/Goal/DropItemForGoal.h"

#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"
#include "World/Level/LightSystem.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <random>
#include <utility>

namespace {
    const float NO_SPOT_SCORE = 99999.99f;

    std::mt19937 &dropRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    Vector3i floored(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    // 0, 1, -1, 2, -2, ...
    int32_t nextOffset(int32_t offset) {
        return (offset <= 0 ? 1 : 0) - offset;
    }

    Actor *actorByRuntimeId(ServerNetworkHandler &owner, uint64_t runtimeId) {
        if (ServerPlayer *player = owner.getPlayerByRuntimeId(runtimeId))
            return player;
        return owner.getActorByRuntimeId(runtimeId);
    }

    bool withinBox(const Vector3f &center, const Vector3f &range, const Vector3f &position) {
        return std::fabs(position.x - center.x) <= range.x && std::fabs(position.y - center.y) <= range.y
               && std::fabs(position.z - center.z) <= range.z;
    }
}

DropItemForGoal::DropItemForGoal(Settings settings, Offer offer)
        : BaseMoveToBlockGoal(settings), mSettings(settings), mOffer(std::move(offer)) {}

bool DropItemForGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    const int32_t left = mCooldownLeft;
    if (left > 0)
        --mCooldownLeft;
    return left <= 0;
}

bool DropItemForGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mReceiverId != 0 && !mDropped;
}

void DropItemForGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mReceiverId = 0;
    mDropped = false;
    mCooldownLeft = mOffer.mCooldownTicks;
    // the spot falls back to where the mob stands: with no spot found it drops right there
    const Vector3i feet = floored(mob.getPosition());
    setTarget(Vector3i(feet.x, feet.y - 1, feet.z));

    const float timeOfDay = LightSystem::calculateCelestialAngle(owner.getLevelFor(mob).getTime());
    if (timeOfDay < mOffer.mTimeOfDayMin || timeOfDay > mOffer.mTimeOfDayMax)
        return;

    const Actor *receiver = _findReceiver(owner, mob);
    if (receiver == nullptr)
        return;

    mReceiverId = receiver->getRuntimeId();
    _findSpotNear(owner, mob, *receiver);
    BaseMoveToBlockGoal::start(owner, mob);
}

void DropItemForGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::stop(owner, mob);
    Actor *receiver = mReceiverId == 0 ? nullptr : actorByRuntimeId(owner, mReceiverId);
    mReceiverId = 0;
    mDropped = false;
    EntityEvents::fireTrigger(owner, mob, mOffer.mOnDropAttempt.get(), receiver);
}

void DropItemForGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    BaseMoveToBlockGoal::tick(owner, mob);

    // the spot's corner, not its middle, is what the game measures from and puts the mob on
    const Vector3i &spot = getTarget();
    const Vector3f corner((float) spot.x, (float) spot.y, (float) spot.z);
    const Vector3f &position = mob.getPosition();
    const float dx = corner.x - position.x;
    const float dy = corner.y - position.y;
    const float dz = corner.z - position.z;
    if (dx * dx + dy * dy + dz * dz > mOffer.mMinimumTeleportDistance) {
        mob.teleport(Vector3f(corner.x + std::floor(mOffer.mTeleportOffset.x),
                              corner.y + std::floor(mOffer.mTeleportOffset.y),
                              corner.z + std::floor(mOffer.mTeleportOffset.z)));
        mob.getNavigation().stop(mob);
    }

    Actor *receiver = actorByRuntimeId(owner, mReceiverId);
    if (receiver == nullptr || !receiver->isAlive()) {
        mReceiverId = 0;
        return;
    }

    if (!mDropped && (isReached() || mob.getNavigation().isDone())) {
        mDropped = true;
        _drop(owner, mob);
    }
    mob.getLookControl().setLookAt(receiver->getPosition());
}

// solid ground with air above
bool DropItemForGoal::isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) {
    Level &level = owner.getLevelFor(mob);
    const BlockState *above = level.peekBlockPtr(position.x, position.y + 1, position.z);
    if (above == nullptr || above->mName != BlockNames::AIR)
        return false;
    const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
    const BlockData *data = state == nullptr ? nullptr : BlockDataTable::find(state->mName.c_str());
    return data != nullptr && data->mSolid;
}

Actor *DropItemForGoal::_findReceiver(ServerNetworkHandler &owner, MobActor &mob) const {
    const Level &level = owner.getLevelFor(mob);
    const Vector3f center = mob.getPosition();
    const Vector3f &range = mOffer.mTargetRange;
    Actor *nearest = nullptr;
    float nearestDistance = 0.0f;

    const auto consider = [&](Actor &candidate) {
        if (&candidate == &mob || !candidate.isAlive() || !candidate.isIn(level)
            || !withinBox(center, range, candidate.getPosition()))
            return;

        const float distance = mob.distanceSquaredTo(candidate);
        if (nearest != nullptr && distance >= nearestDistance)
            return;
        for (const NearestAttackableTargetGoal::Entry &entry: mOffer.mEntries) {
            if (entry.mMaxDistance > 0.0f && distance > entry.mMaxDistance * entry.mMaxDistance)
                continue;
            if (entry.mFilters != nullptr && !EntityFilter::test(*entry.mFilters, owner, mob, &candidate))
                continue;
            nearest = &candidate;
            nearestDistance = distance;
            return;
        }
    };

    const float radius = std::sqrt(range.x * range.x + range.y * range.y + range.z * range.z);
    owner.forEachPlayerNear(center, radius, consider);
    owner.forEachActorNear(center, radius, consider);
    return nearest;
}

// the reachable spot around the entity whose squared distance to it is closest to the offering distance
bool DropItemForGoal::_findSpotNear(ServerNetworkHandler &owner, MobActor &mob, const Actor &receiver) {
    if (mSettings.mSearchHeight < 0 || mSettings.mSearchRange <= 0)
        return false;

    const Vector3i origin = floored(receiver.getPosition());
    float best = NO_SPOT_SCORE;
    bool found = false;
    for (int32_t dy = 0; dy <= mSettings.mSearchHeight; dy = nextOffset(dy)) {
        const int32_t y = origin.y - 1 + dy;
        for (int32_t ring = 0; ring < mSettings.mSearchRange; ++ring) {
            for (int32_t dx = 0; dx <= ring; dx = nextOffset(dx)) {
                const bool edge = dx <= -ring || dx >= ring;
                for (int32_t dz = edge ? 0 : ring; dz <= ring; dz = nextOffset(dz)) {
                    const Vector3i position(origin.x + dx, y, origin.z + dz);
                    if (!isValidTarget(owner, mob, position))
                        continue;

                    const int32_t ox = origin.x - position.x;
                    const int32_t oy = origin.y - position.y;
                    const int32_t oz = origin.z - position.z;
                    const float score = std::fabs(mOffer.mOfferingDistance - (float) (ox * ox + oy * oy + oz * oz));
                    if (score >= best || !canReach(owner, mob, position))
                        continue;

                    best = score;
                    setTarget(position);
                    found = true;
                }
            }
        }
    }
    return found;
}

void DropItemForGoal::_drop(ServerNetworkHandler &owner, MobActor &mob) {
    if (mOffer.mDropChance <= 0.0f || std::uniform_real_distribution<float>(0.0f, 1.0f)(dropRandom())
                                                 > mOffer.mDropChance)
        return;

    Level &level = owner.getLevelFor(mob);
    for (const ItemStack &stack: mob.rollLoot(owner, mOffer.mLootTable))
        owner.dropItem(level, mob.getPosition(), stack, ItemActorHandler::randomDropMotion(),
                       mOffer.mPickupDelayTicks);
}
