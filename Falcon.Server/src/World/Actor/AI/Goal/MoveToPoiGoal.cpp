#include "World/Actor/AI/Goal/MoveToPoiGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Level.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Village.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <random>

namespace {
    // a new try every 60 to 119 calls
    const int32_t SEARCH_INTERVAL = 60;
    const int32_t MAX_TRAVEL_TICKS = 1200;
    const int32_t MAX_STAY_TICKS = 1200;
    const int32_t GIVE_UP_TICKS = 160;
    const int32_t REPATH_INTERVAL = 100;
    // a failed trip shows angry particles over the point twice, then stays quiet
    const int16_t NOISY_FAILED_TRIPS = 2;
    const int32_t PARTICLE_EVENT = 2009;
    const int32_t ANGRY_PARTICLE_DATA = 40;
    const int32_t PARTICLE_COUNT = 5;

    std::mt19937 &poiRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float nextFloat() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(poiRandom());
    }

    Vector3i floored(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }

    AxisAlignedBB boxOf(const MobActor &mob) {
        const Vector3f &position = mob.getPosition();
        const ActorSize size = mob.getSize();
        const float half = size.mWidth * 0.5f;
        return AxisAlignedBB(position.x - half, position.y, position.z - half, position.x + half,
                             position.y + size.mHeight, position.z + half);
    }

    AxisAlignedBB blockBox(Level &level, const Vector3i &position) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        if (state == nullptr)
            return AxisAlignedBB((float) position.x, (float) position.y, (float) position.z, (float) position.x,
                                 (float) position.y, (float) position.z);
        return BlockShape::getShapeAt(*state, position.x, position.y, position.z);
    }

    void showFailedTrip(ServerNetworkHandler &owner, Level &level, const PoiInstance &poi) {
        const Vector3i &block = poi.mPosition;
        const AxisAlignedBB box = blockBox(level, block);
        for (int32_t index = 0; index < PARTICLE_COUNT; ++index) {
            // z is rolled first, then y, then x
            const float rollZ = nextFloat();
            const float rollY = nextFloat();
            const float rollX = nextFloat();
            const Vector3f particle(rollX * (box.mMaxX - box.mMinX) + (float) block.x + 0.2f,
                                    rollY * (box.mMaxY - box.mMinY) + (float) block.y + 1.0f,
                                    rollZ * (box.mMaxZ - box.mMinZ) + (float) block.z + 0.2f);
            owner.broadcastLevelEvent(level, PARTICLE_EVENT, particle, ANGRY_PARTICLE_DATA);
        }
    }
}

MoveToPoiGoal::MoveToPoiGoal(float speed, PoiType type) : mSpeed(speed), mType(type) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

std::shared_ptr<PoiInstance> MoveToPoiGoal::findClaimedPoi(const MobActor &mob, PoiType type) {
    const MobDweller &dweller = mob.getDweller();
    const std::shared_ptr<Village> village = dweller.isActive() ? dweller.getVillage() : nullptr;
    return village == nullptr ? nullptr : village->getClaimedPoi(mob.getUniqueId(), type);
}

bool MoveToPoiGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mGiveUpTicks > 0) {
        --mGiveUpTicks;
        return false;
    }
    if (mIntervalTicks > 0) {
        --mIntervalTicks;
        return false;
    }
    mIntervalTicks = SEARCH_INTERVAL + std::uniform_int_distribution<int32_t>(0, SEARCH_INTERVAL - 1)(poiRandom());

    const std::shared_ptr<PoiInstance> poi = findClaimedPoi(mob, mType);
    if (poi == nullptr)
        return false;

    mTargetBlock = poi->mPosition;
    mTarget = Vector3f((float) mTargetBlock.x, (float) mTargetBlock.y, (float) mTargetBlock.z);
    mRadiusSquared = poi->mRadius * poi->mRadius;
    mUseBoundingBox = poi->mUseBoundingBox;
    if (mUseBoundingBox) {
        const AxisAlignedBB box = blockBox(owner.getLevelFor(mob), mTargetBlock);
        mTarget = Vector3f(box.mMinX + (box.mMaxX - box.mMinX) * 0.5f, box.mMinY + (box.mMaxY - box.mMinY) * 0.5f,
                           box.mMinZ + (box.mMaxZ - box.mMinZ) * 0.5f);
        mTargetBox = box.expand(poi->mRadius, poi->mRadius, poi->mRadius);
    }
    return true;
}

bool MoveToPoiGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (mGiveUpTicks != 0 || mStayTicks > MAX_STAY_TICKS || mTravelTicks > MAX_TRAVEL_TICKS)
        return false;
    return findClaimedPoi(mob, mType) != nullptr;
}

void MoveToPoiGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTravelTicks = 0;
    mStayTicks = 0;
    mReached = false;
    mLastPathStart = floored(mob.getPosition());
    moveToTarget(mob);
}

void MoveToPoiGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const std::shared_ptr<PoiInstance> poi = findClaimedPoi(mob, mType);
    if (poi != nullptr) {
        if (mGiveUpTicks != 0 || mTravelTicks > MAX_TRAVEL_TICKS) {
            ++poi->mFailedTrips;
            if (poi->mFailedTrips <= NOISY_FAILED_TRIPS)
                showFailedTrip(owner, owner.getLevelFor(mob), *poi);
        } else {
            poi->mFailedTrips = 0;
        }
    }

    mob.getNavigation().stop(mob);
    mTravelTicks = 0;
}

void MoveToPoiGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    if (_isReached(mob)) {
        mReached = true;
        ++mStayTicks;
        return;
    }

    mReached = false;
    ++mTravelTicks;
    // a point reached by its box only repaths on the interval; the others also when the path ran out
    const bool pathOver = !mUseBoundingBox && mob.getNavigation().isDone();
    if (!pathOver && mTravelTicks % REPATH_INTERVAL != 0)
        return;

    moveToTarget(mob);
    // still on the block it set off from at the last repath: stuck
    const Vector3i here = floored(mob.getPosition());
    if (here == mLastPathStart) {
        mGiveUpTicks = GIVE_UP_TICKS;
        return;
    }
    mLastPathStart = here;
}

void MoveToPoiGoal::moveToTarget(MobActor &mob) {
    const Vector3f destination = mUseBoundingBox ? mTarget : Vector3f(mTarget.x + 0.5f, mTarget.y, mTarget.z + 0.5f);
    mob.getNavigation().moveTo(destination, mSpeed);
}

bool MoveToPoiGoal::_isReached(const MobActor &mob) const {
    if (mUseBoundingBox) {
        const AxisAlignedBB box = boxOf(mob);
        return box.mMaxX > mTargetBox.mMinX && mTargetBox.mMaxX > box.mMinX && box.mMaxY > mTargetBox.mMinY
               && mTargetBox.mMaxY > box.mMinY && box.mMaxZ > mTargetBox.mMinZ && mTargetBox.mMaxZ > box.mMinZ;
    }

    const Vector3f &position = mob.getPosition();
    const float dx = mTarget.x + 0.5f - position.x;
    const float dy = mTarget.y + 0.5f - (position.y + mob.getSize().mHeight * 0.5f);
    const float dz = mTarget.z + 0.5f - position.z;
    return dx * dx + dy * dy + dz * dz <= mRadiusSquared;
}
