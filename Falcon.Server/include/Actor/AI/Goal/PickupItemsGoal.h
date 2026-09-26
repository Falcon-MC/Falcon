#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>

class ItemActor;

class PickupItemsGoal : public Goal {
public:
    PickupItemsGoal(float speed, float maxDistance, float goalRadius, int32_t hurtCooldownTicks);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _isAvailable(ServerNetworkHandler &owner, const MobActor &mob, const ItemActor &item) const;

    ItemActor *_findItem(ServerNetworkHandler &owner, const MobActor &mob) const;

    ItemActor *_currentItem(ServerNetworkHandler &owner) const;

    void _pickUp(ServerNetworkHandler &owner, MobActor &mob, ItemActor &item);

    float mSpeed;
    float mMaxDistance;
    float mGoalRadius;
    int32_t mHurtCooldownTicks;
    uint64_t mItemRuntimeId = 0;
    int32_t mRepathTicks = 0;
};
