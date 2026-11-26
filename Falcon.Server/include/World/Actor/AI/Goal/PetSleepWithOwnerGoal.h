#pragma once

#include "World/Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

// minecraft:behavior.pet_sleep_with_owner: a tamed pet walks onto the bed its owner sleeps in, settles and lies
// down; the owner waking up runs the pet's minecraft:on_wake_with_owner
class PetSleepWithOwnerGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        // blocks between the pet's and the owner's feet, up or down
        int32_t mSearchHeight = 1;
        // compared with the squared distance to the bed, as the game does
        float mGoalRadius = 0.5f;
    };

    explicit PetSleepWithOwnerGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    enum class Phase : uint8_t { Walking, Settling, Resting };

    void _settle(MobActor &mob);

    Settings mSettings;
    Vector3i mBed;
    Vector3f mBedCenter;
    Phase mPhase = Phase::Walking;
    int32_t mSettleTicks = 0;
};
