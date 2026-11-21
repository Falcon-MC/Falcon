#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>

class ItemActor;

class PickupItemsGoal : public Goal {
public:
    PickupItemsGoal(float speed, float maxDistance, float goalRadius, int32_t hurtCooldownTicks,
                    bool stopIfHolding = false, std::shared_ptr<json::Value> onStart = nullptr,
                    std::shared_ptr<json::Value> onEnd = nullptr);

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
    bool mStopIfHolding;
    std::shared_ptr<json::Value> mOnStart;
    std::shared_ptr<json::Value> mOnEnd;
    uint64_t mItemRuntimeId = 0;
    int32_t mRepathTicks = 0;
};
