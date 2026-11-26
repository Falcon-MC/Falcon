#include "World/Actor/AI/Goal/PetSleepWithOwnerGoal.h"

#include "World/Actor/AI/Control/LookControl.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/BedBlock.h"
#include "World/Level/Level.h"
#include "Core/Math/VectorMath.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const char *const ON_WAKE_COMPONENT = "minecraft:on_wake_with_owner";

    // on the bed the pet brakes and turns to it for the first three quarters of these ticks, then lies down
    const int32_t SETTLE_TICKS = 20;
    const float SETTLE_MOTION_KEPT = 0.5f;
    const float SETTLE_MAX_TURN = 30.0f;

    Vector3f centerOf(const MobActor &mob) {
        const Vector3f &position = mob.getPosition();
        return Vector3f(position.x, position.y + mob.getSize().mHeight * 0.5f, position.z);
    }

    BedBlockActor *bedActorAt(ServerNetworkHandler &owner, const MobActor &mob, const Vector3i &bed) {
        return owner.getLevelFor(mob).getBlockActors().find<BedBlockActor>(bed);
    }
}

PetSleepWithOwnerGoal::PetSleepWithOwnerGoal(Settings settings) : mSettings(settings) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look);
}

bool PetSleepWithOwnerGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *player = mob.getOwner(owner);
    if (player == nullptr || !player->isSleeping() || !mob.isTamed() || mob.isSitting())
        return false;

    Level &level = owner.getLevelFor(mob);
    if (!player->isIn(level)
        || std::fabs(mob.getPosition().y - player->getPosition().y) > (float) mSettings.mSearchHeight)
        return false;

    const Vector3i bed = player->getSleepingPosition();
    const BlockState *state = level.peekBlockPtr(bed.x, bed.y, bed.z);
    if (state == nullptr || !BedBlock::matches(state->mName))
        return false;

    BedBlockActor *bedActor = bedActorAt(owner, mob, bed);
    if (bedActor != nullptr && !bedActor->claimPet(mob.getUniqueId()))
        return false;

    mBed = bed;
    mBedCenter = VectorMath::centerOf(bed);
    return true;
}

bool PetSleepWithOwnerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *player = mob.getOwner(owner);
    return player != nullptr && player->isSleeping() && !mob.isSitting();
}

void PetSleepWithOwnerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mPhase = Phase::Walking;
    mob.getNavigation().moveTo(mBedCenter, mSettings.mSpeed);
}

void PetSleepWithOwnerGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    const bool rested = mPhase == Phase::Resting;
    mPhase = Phase::Walking;
    if (rested)
        mob.setRestingFlag(owner, false);
    mob.getNavigation().stop(mob);
    if (BedBlockActor *bedActor = bedActorAt(owner, mob, mBed))
        bedActor->releasePet(mob.getUniqueId());

    // only a pet lying on the bed when its owner gets up
    ServerPlayer *player = mob.getOwner(owner);
    if (rested && player != nullptr && !player->isSleeping())
        EntityEvents::fireTrigger(owner, mob, mob.getComponent(ON_WAKE_COMPONENT), player);
}

void PetSleepWithOwnerGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    switch (mPhase) {
        case Phase::Walking:
            // the game compares the squared distance with the radius itself
            if (VectorMath::distanceSquared(mBedCenter, centerOf(mob)) < mSettings.mGoalRadius) {
                mPhase = Phase::Settling;
                mSettleTicks = SETTLE_TICKS;
            }
            break;
        case Phase::Settling:
            _settle(mob);
            if (mSettleTicks > 0)
                break;
            mob.getNavigation().stop(mob);
            mob.setRestingFlag(owner, true);
            mPhase = Phase::Resting;
            break;
        case Phase::Resting:
            break;
    }
}

void PetSleepWithOwnerGoal::_settle(MobActor &mob) {
    if (mSettleTicks <= SETTLE_TICKS / 4) {
        mSettleTicks = std::max(0, mSettleTicks - 1);
        return;
    }

    --mSettleTicks;
    const Vector3f &motion = mob.getMotion();
    mob.setMotion(Vector3f(motion.x * SETTLE_MOTION_KEPT, motion.y * SETTLE_MOTION_KEPT, motion.z * SETTLE_MOTION_KEPT));
    Vector3f rotation = mob.getRotation();
    const float turn = VectorMath::wrapDegrees(LookControl::yawTowards(mob.getPosition(), mBedCenter) - rotation.y);
    rotation.y += std::clamp(turn, -SETTLE_MAX_TURN, SETTLE_MAX_TURN);
    rotation.z = rotation.y;
    mob.setRotation(rotation);
}
