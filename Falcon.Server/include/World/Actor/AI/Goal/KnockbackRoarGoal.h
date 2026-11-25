#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "World/Actor/Definition/EntityFilter.h"

#include <cstdint>
#include <memory>

namespace json {
    class Value;
}

class Actor;

// minecraft:behavior.knockback_roar: the ravager's roar, pushing back and hurting what stands around it
class KnockbackRoarGoal : public Goal {
public:
    // times in ticks
    struct Settings {
        int32_t mDuration = 20;
        int32_t mAttackTime = 10;
        int32_t mDamage = 6;
        int32_t mHorizontalStrength = 4;
        float mHeightCap = 0.4f;
        int32_t mRange = 4;
        int32_t mCooldown = 2;
        CompiledEntityFilter mKnockbackFilters;
        CompiledEntityFilter mDamageFilters;
        std::shared_ptr<json::Value> mOnRoarEnd;
    };

    explicit KnockbackRoarGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    void _roar(ServerNetworkHandler &owner, MobActor &mob);

    void _strike(ServerNetworkHandler &owner, MobActor &mob, Actor &target, float sourceX, float sourceZ);

    Settings mSettings;
    int32_t mCooldownTicks = 0;
    int32_t mTicks = 0;
};
