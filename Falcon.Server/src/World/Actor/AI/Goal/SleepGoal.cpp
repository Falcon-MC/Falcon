#include "World/Actor/AI/Goal/SleepGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BedBlock.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "World/Village/PoiInstance.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <memory>

namespace {
    // the game's table by bed direction (south, west, north, east): where on the bed the sleeper lies, the step
    // towards the foot, and the yaw it faces
    struct BedPose {
        float mX;
        float mZ;
        float mStepX;
        float mStepZ;
        float mYaw;
    };

    const BedPose BED_POSES[] = {{0.5f, 0.875f, 0.0f, -1.0f, 180.0f},
                                 {0.125f, 0.5f, 1.0f, 0.0f, 270.0f},
                                 {0.5f, 0.125f, 0.0f, 1.0f, 0.0f},
                                 {0.875f, 0.5f, -1.0f, 0.0f, 90.0f}};

    int32_t bedDirection(const BlockState &state) {
        switch (BedBlock::headFace(state)) {
            case RedstoneFace::WEST:
                return 1;
            case RedstoneFace::NORTH:
                return 2;
            case RedstoneFace::EAST:
                return 3;
            default:
                return 0;
        }
    }

    Vector3i floored(const Vector3f &position) {
        return Vector3i((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                        (int32_t) std::floor(position.z));
    }
}

SleepGoal::SleepGoal(Settings settings) : MoveToPoiGoal(settings.mSpeed, PoiType::Bed), mSettings(settings) {}

bool SleepGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (!MoveToPoiGoal::canUse(owner, mob))
        return false;
    if (!mSettings.mCanSleepWhileRiding && mob.isRiding())
        return false;

    const std::shared_ptr<PoiInstance> bed = findClaimedPoi(mob, PoiType::Bed);
    if (bed == nullptr)
        return false;

    if (mCoolingDown) {
        if (++mCooldownCalls <= mSettings.mTimeoutTicks)
            return false;
        mCoolingDown = false;
    }

    mBed = bed->mPosition;
    const BlockState *state = owner.getLevelFor(mob).peekBlockPtr(mBed.x, mBed.y, mBed.z);
    const BedPose pose = state != nullptr && BedBlock::matches(state->mName) ? BED_POSES[bedDirection(*state)]
                                                                             : BedPose{0.5f, 0.5f, 0.0f, 0.0f, 0.0f};
    mSleepPosition = Vector3f((float) mBed.x + pose.mX + pose.mStepX, (float) mBed.y + mSettings.mYOffset,
                              (float) mBed.z + pose.mZ + pose.mStepZ);
    mSleepYaw = pose.mYaw;
    return true;
}

bool SleepGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    // in bed the stay limit no longer applies: the villager sleeps until the schedule ends the goal
    const bool keep = isReached() ? findClaimedPoi(mob, PoiType::Bed) != nullptr
                                  : MoveToPoiGoal::canContinueToUse(owner, mob);
    mWoken = !keep || mob.getFlags().get(ActorFlag::WantsToWake);
    return !mWoken;
}

void SleepGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.getFlags().get(ActorFlag::Sleeping)) {
        // a night in bed heals; a villager woken up gets nothing
        if (!mWoken && mob.isAlive())
            mob.setHealth(mob.getMaxHealth());

        Level &level = owner.getLevelFor(mob);
        Vector3i standUp = floored(mSleepPosition);
        const BlockState *state = level.peekBlockPtr(mBed.x, mBed.y, mBed.z);
        if (state != nullptr && BedBlock::matches(state->mName)) {
            Vector3i found;
            standUp = BedBlock::findStandUpPosition(level, mBed, mLayDownPosition, found)
                              ? found
                              : Vector3i(standUp.x, standUp.y + 1, standUp.z);
        }
        mob.teleport(Vector3f((float) standUp.x + 0.5f, (float) standUp.y + 0.5f, (float) standUp.z + 0.5f));
        mCoolingDown = true;
        mob.setSleepingFlag(owner, false);
    }

    mCooldownCalls = 0;
    mWoken = false;
    mob.setWantsToWake(owner, false);
    MoveToPoiGoal::stop(owner, mob);
}

void SleepGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    MoveToPoiGoal::tick(owner, mob);
    if (!mob.getFlags().get(ActorFlag::Sleeping)) {
        if (!isReached())
            return;

        mob.setSleepingFlag(owner, true);
        mLayDownPosition = mob.getPosition();
        mob.getNavigation().stop(mob);
    }

    mob.teleport(mSleepPosition);
    mob.setRotation(Vector3f(0.0f, mSleepYaw, mSleepYaw));
}
