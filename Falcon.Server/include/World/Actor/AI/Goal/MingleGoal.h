#pragma once

#include "World/Actor/AI/Goal/MoveToPoiGoal.h"

#include <cstdint>
#include <string>

// minecraft:behavior.mingle: a villager walks to its village's meeting point, picks another villager there and
// stands chatting with it
class MingleGoal final : public MoveToPoiGoal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        int32_t mDurationTicks = 0;
        int32_t mCooldownTicks = 0;
        float mMingleDistance = 0.0f;
        std::string mPartnerType;
    };

    explicit MingleGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    Settings mSettings;
    int32_t mCooldown = 0;
    int32_t mTicksLeft = 0;
    int32_t mSearchTicks = 0;
    int64_t mPartnerId = 0;
};
