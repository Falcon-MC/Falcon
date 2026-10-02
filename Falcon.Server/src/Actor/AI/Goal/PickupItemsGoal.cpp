#include "Actor/AI/Goal/PickupItemsGoal.h"

#include "Actor/Misc/ItemActor.h"
#include "Actor/Mob/Component/MobShareables.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const int32_t REPATH_INTERVAL = 10;

    float distanceSquared(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dy = left.y - right.y;
        const float dz = left.z - right.z;
        return dx * dx + dy * dy + dz * dz;
    }
}

PickupItemsGoal::PickupItemsGoal(float speed, float maxDistance, float goalRadius, int32_t hurtCooldownTicks)
        : mSpeed(speed), mMaxDistance(maxDistance), mGoalRadius(goalRadius), mHurtCooldownTicks(hurtCooldownTicks) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move);
}

bool PickupItemsGoal::_isAvailable(ServerNetworkHandler &owner, const MobActor &mob, const ItemActor &item) const {
    return !item.isRemoved() && item.canPickup() && item.getDimension() == mob.getDimension()
           && distanceSquared(item.getPosition(), mob.getPosition()) <= mMaxDistance * mMaxDistance
           && MobShareables::wants(owner, mob, item.getItem());
}

ItemActor *PickupItemsGoal::_findItem(ServerNetworkHandler &owner, const MobActor &mob) const {
    ItemActor *nearest = nullptr;
    float nearestDistance = 0.0f;
    for (const std::unique_ptr<ItemActor> &item: owner.getItemEntities()) {
        if (!_isAvailable(owner, mob, *item))
            continue;

        const float distance = distanceSquared(item->getPosition(), mob.getPosition());
        if (nearest == nullptr || distance < nearestDistance) {
            nearest = item.get();
            nearestDistance = distance;
        }
    }
    return nearest;
}

ItemActor *PickupItemsGoal::_currentItem(ServerNetworkHandler &owner) const {
    for (const std::unique_ptr<ItemActor> &item: owner.getItemEntities()) {
        if (item->getRuntimeId() == mItemRuntimeId)
            return item.get();
    }
    return nullptr;
}

bool PickupItemsGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (owner.getCurrentTick() - mob.getLastHurtTick() < mHurtCooldownTicks)
        return false;

    const ItemActor *item = _findItem(owner, mob);
    if (item == nullptr)
        return false;

    mItemRuntimeId = item->getRuntimeId();
    return true;
}

bool PickupItemsGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    const ItemActor *item = _currentItem(owner);
    return item != nullptr && _isAvailable(owner, mob, *item)
           && owner.getCurrentTick() - mob.getLastHurtTick() >= mHurtCooldownTicks;
}

void PickupItemsGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mRepathTicks = 0;
}

void PickupItemsGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mItemRuntimeId = 0;
    mob.getNavigation().stop(mob);
}

void PickupItemsGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    ItemActor *item = _currentItem(owner);
    if (item == nullptr)
        return;

    if (distanceSquared(item->getPosition(), mob.getPosition()) <= mGoalRadius * mGoalRadius) {
        _pickUp(owner, mob, *item);
        return;
    }

    if (--mRepathTicks > 0)
        return;

    mRepathTicks = REPATH_INTERVAL;
    mob.getNavigation().moveTo(item->getPosition(), mSpeed);
}

void PickupItemsGoal::_pickUp(ServerNetworkHandler &owner, MobActor &mob, ItemActor &item) {
    ItemStack single = item.getItem();
    single.mCount = 1;
    if (!MobShareables::take(owner, mob, single))
        return;

    ItemStack &remaining = item.getItem();
    remaining.mCount -= 1;
    if (remaining.mCount <= 0)
        ItemActorHandler::collect(owner, item, mob);
    else
        ItemActorHandler::refresh(owner, item);
    mItemRuntimeId = 0;
}
