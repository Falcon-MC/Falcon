#include "World/Actor/AI/Goal/MoveTowardsDwellingRestrictionGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"
#include "World/Village/Village.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <memory>
#include <random>

namespace {
    // a fresh search every 50 to 99 ticks
    const int32_t SEARCH_INTERVAL = 50;
    const int32_t GIVE_UP_TICKS = 1200;
    // the goal radius is 0.5; the game compares the squared distance with twice its square
    const float NEAR_TARGET_SQUARED = 2.0f * 0.5f * 0.5f;

    std::mt19937 &dwellingRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t between(int32_t minimum, int32_t maximum) {
        if (maximum <= minimum)
            return minimum;
        return minimum + std::uniform_int_distribution<int32_t>(0, maximum - minimum - 1)(dwellingRandom());
    }

    std::shared_ptr<Village> villageOf(const MobActor &mob) {
        return mob.getDweller().isActive() ? mob.getDweller().getVillage() : nullptr;
    }
}

MoveTowardsDwellingRestrictionGoal::MoveTowardsDwellingRestrictionGoal(float speed) : mSpeed(speed) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveTowardsDwellingRestrictionGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (mIntervalLeft > 0) {
        --mIntervalLeft;
        return false;
    }
    mIntervalLeft = SEARCH_INTERVAL + std::uniform_int_distribution<int32_t>(0, SEARCH_INTERVAL - 1)(dwellingRandom());

    // a village without a claimed point has no box yet
    const std::shared_ptr<Village> village = villageOf(mob);
    return village != nullptr && !std::isnan(village->getBounds().mMinX)
           && !village->contains(0.0f, mob.getPosition());
}

bool MoveTowardsDwellingRestrictionGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    const std::shared_ptr<Village> village = villageOf(mob);
    if (village == nullptr || mTicksMoving > GIVE_UP_TICKS || mob.getNavigation().isDone())
        return false;

    const Vector3f &position = mob.getPosition();
    const float dx = std::floor(mTarget.x) + 0.5f - position.x;
    const float dy = std::floor(mTarget.y) + 0.5f - position.y;
    const float dz = std::floor(mTarget.z) + 0.5f - position.z;
    if (dx * dx + dy * dy + dz * dz >= NEAR_TARGET_SQUARED)
        return true;
    return mReachedInside && !village->contains(0.0f, position);
}

void MoveTowardsDwellingRestrictionGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    mTicksMoving = 0;
    mReachedInside = false;
    const std::shared_ptr<Village> village = villageOf(mob);
    if (village == nullptr)
        return;

    // x then z inside the box, then the height map of that column
    const AxisAlignedBB &bounds = village->getBounds();
    const int32_t x = between((int32_t) bounds.mMinX, (int32_t) bounds.mMaxX);
    const int32_t z = between((int32_t) bounds.mMinZ, (int32_t) bounds.mMaxZ);
    const int32_t y = owner.getLevelFor(mob).getHeightAt(x, z);
    mTarget = Vector3f((float) x, (float) y, (float) z);
    mob.getNavigation().moveTo(mTarget, mSpeed);
}

void MoveTowardsDwellingRestrictionGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mob.getNavigation().stop(mob);
}

void MoveTowardsDwellingRestrictionGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    ++mTicksMoving;
    const std::shared_ptr<Village> village = villageOf(mob);
    if (!mReachedInside && village != nullptr && village->contains(0.0f, mob.getPosition()))
        mReachedInside = true;
}
