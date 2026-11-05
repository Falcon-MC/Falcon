#include "World/Actor/AI/Goal/MoveToVillageGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Level/Level.h"
#include "World/Village/Village.h"
#include "World/Village/VillageManager.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    // the base move-to goal (sub_14704F910): give up after 1200 ticks of travel or 1200 ticks at the target
    const int32_t MAX_TRAVEL_TICKS = 1200;
    const int32_t MAX_TICKS_AT_TARGET = 1200;
    // a new path is asked for every 100 ticks of travel (vtable slot 17, sub_140B49D10)
    const int32_t REPATH_INTERVAL = 100;
    // the start check runs every 50 to 99 ticks (sub_1474FA230 sets the base 200 to 50)
    const int32_t START_INTERVAL = 50;
    // a mob without the dweller component takes the closest village whose box grown by 64 holds it (sub_1474FA4A0)
    const int32_t VILLAGE_SEARCH_RADIUS = 64;
    const float LEG_FRACTION = 0.75f;
    const float DIRECTION_EPSILON = 0.0001f;
    const float TARGET_HEIGHT_OFFSET = 1.0f;
    const float BLOCK_CENTER = 0.5f;

    std::mt19937 &moveToVillageRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t nextInt(int32_t bound) {
        return bound <= 0 ? 0 : std::uniform_int_distribution<int32_t>(0, bound - 1)(moveToVillageRandom());
    }

    bool isInside(const AxisAlignedBB &bounds, const Vector3f &position) {
        return bounds.mMinX <= position.x && position.x <= bounds.mMaxX && bounds.mMinY <= position.y
               && position.y <= bounds.mMaxY && bounds.mMinZ <= position.z && position.z <= bounds.mMaxZ;
    }

    Vector3i blockOf(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

MoveToVillageGoal::MoveToVillageGoal(const Settings &settings)
        : mSettings(settings), mGoalRadiusSquared(settings.mGoalRadius * settings.mGoalRadius) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool MoveToVillageGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCooldown > 0) {
        --mCooldown;
        return false;
    }

    if (mStartDelay > 0) {
        --mStartDelay;
        return false;
    }
    mStartDelay = START_INTERVAL + nextInt(START_INTERVAL);

    const std::shared_ptr<Village> village = _findVillage(owner, mob);
    return village != nullptr && !isInside(village->getBounds(), mob.getPosition());
}

bool MoveToVillageGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mCooldown != 0 || mTicksAtTarget > MAX_TICKS_AT_TARGET || mTravelTicks > MAX_TRAVEL_TICKS)
        return false;

    const std::shared_ptr<Village> village = _findVillage(owner, mob);
    if (village == nullptr)
        return false;

    const Vector3i targetBlock = blockOf(mTarget);
    if (!isInside(village->getBounds(), Vector3f((float) targetBlock.x, (float) targetBlock.y, (float) targetBlock.z)))
        return false;

    const Vector3f &position = mob.getPosition();
    const bool outside = !isInside(village->getBounds(), position);
    const Vector3i goal = blockOf(_targetPosition());
    const float dx = (float) goal.x + BLOCK_CENTER - position.x;
    const float dy = (float) goal.y + BLOCK_CENTER - position.y;
    const float dz = (float) goal.z + BLOCK_CENTER - position.z;
    // close to the target the goal only goes on for a mob that has been in the village and walked out again
    if (mGoalRadiusSquared + mGoalRadiusSquared > dx * dx + dy * dy + dz * dz)
        return mEnteredVillage && outside;
    return true;
}

void MoveToVillageGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    // sub_1474FA350: the path itself is asked for by the first tick
    const Vector3i target = _selectTarget(owner, mob);
    mTarget = Vector3f((float) target.x, (float) target.y, (float) target.z);
}

void MoveToVillageGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    // sub_1470507C0 resets the travel time only, the time at the target and the entered flag are kept
    (void) owner;
    mob.getNavigation().stop(mob);
    mTravelTicks = 0;
    mPathCheckPending = false;
}

void MoveToVillageGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mPathCheckPending)
        _checkRepeatedPath(mob);

    // the base tick (sub_1470508E0) measures to the middle of the mob's box
    const Vector3f target = _targetPosition();
    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(mob);
    const Vector3f &position = mob.getPosition();
    const float dx = target.x - position.x;
    const float dy = target.y - (box.mMinY + (box.mMaxY - box.mMinY) * 0.5f);
    const float dz = target.z - position.z;
    if (dx * dx + dy * dy + dz * dz <= mGoalRadiusSquared) {
        ++mTicksAtTarget;
    } else {
        ++mTravelTicks;
        // a finished path stops the navigation here, so isDone covers both no path and a walked one
        if (mob.getNavigation().isDone() || mTravelTicks % REPATH_INTERVAL == 0) {
            _moveCloser(owner, mob);
            mPathCheckPending = true;
        }
    }

    // a leg whose end is within the squared goal radius is extended before the mob stops (sub_143A08830)
    const Path *path = mob.getNavigation().getPath();
    if (path != nullptr && !path->isEmpty()) {
        const Vector3f &end = path->last();
        const float ex = end.x - position.x;
        const float ey = end.y - position.y;
        const float ez = end.z - position.z;
        if (mGoalRadiusSquared * mGoalRadiusSquared > ex * ex + ey * ey + ez * ez)
            _moveCloser(owner, mob);
    }

    if (mEnteredVillage)
        return;

    const std::shared_ptr<Village> village = _findVillage(owner, mob);
    if (village != nullptr && isInside(village->getBounds(), mob.getPosition()))
        mEnteredVillage = true;
}

std::shared_ptr<Village> MoveToVillageGoal::_findVillage(ServerNetworkHandler &owner, const MobActor &mob) const {
    // a dweller only looks at its own village, even when it has none
    if (mob.getDweller().isActive())
        return mob.getDweller().getVillage();

    const Vector3i origin = blockOf(mob.getPosition());
    return owner.getLevelFor(mob).getVillageManager().getClosestVillage(origin, VILLAGE_SEARCH_RADIUS);
}

Vector3i MoveToVillageGoal::_selectTarget(ServerNetworkHandler &owner, const MobActor &mob) const {
    // sub_1474FAB80: x, z then y are drawn in the village box, and y is replaced by the ground below it
    const std::shared_ptr<Village> village = _findVillage(owner, mob);
    if (village == nullptr)
        return Vector3i(0, 0, 0);

    const AxisAlignedBB &bounds = village->getBounds();
    const auto draw = [](float minimum, float maximum) {
        const int32_t low = (int32_t) minimum;
        const int32_t high = (int32_t) maximum;
        return high > low ? low + nextInt(high - low) : low;
    };
    const int32_t x = draw(bounds.mMinX, bounds.mMaxX);
    const int32_t z = draw(bounds.mMinZ, bounds.mMaxZ);
    const int32_t y = draw(bounds.mMinY, bounds.mMaxY);
    return Vector3i(x, owner.getLevelFor(mob).getAboveTopSolidBlock(x, y, z), z);
}

Vector3f MoveToVillageGoal::_targetPosition() const {
    // the base goal aims one block above the chosen block (sub_147050CF0)
    return Vector3f(mTarget.x, mTarget.y + TARGET_HEIGHT_OFFSET, mTarget.z);
}

void MoveToVillageGoal::_moveCloser(ServerNetworkHandler &owner, MobActor &mob) const {
    // sub_1474FA890: a leg of three quarters of the follow range towards the target, on the ground at its end
    const Vector3f &position = mob.getPosition();
    const float dx = mTarget.x - position.x;
    const float dy = mTarget.y - position.y;
    const float dz = mTarget.z - position.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float scale = length < DIRECTION_EPSILON ? 0.0f : mSettings.mFollowRange * LEG_FRACTION / length;
    const Vector3f end(position.x + dx * scale, position.y + dy * scale, position.z + dz * scale);
    const Vector3i block = blockOf(end);
    const float ground = (float) owner.getLevelFor(mob).getAboveTopSolidBlock(block.x, block.y, block.z);
    mob.getNavigation().moveTo(Vector3f(end.x, ground, end.z), mSettings.mSpeed);
}

void MoveToVillageGoal::_checkRepeatedPath(const MobActor &mob) {
    // sub_143A08990, checked once the new path is computed: no path, or a path starting from the block the last one
    // started from, puts the goal on cooldown. Falcon paths keep no start node, so the mob's block stands for it.
    const PathNavigation &navigation = mob.getNavigation();
    if (navigation.getPath() == nullptr && !navigation.isDone())
        return;

    mPathCheckPending = false;
    const Vector3i start = blockOf(mob.getPosition());
    if (navigation.isDone() || (start.x == mLastPathStart.x && start.y == mLastPathStart.y
                                && start.z == mLastPathStart.z)) {
        mCooldown = mSettings.mCooldownTicks;
        return;
    }
    mLastPathStart = start;
}
