#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class Level;

class RandomHoverGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        int32_t mHorizontalRange = 0;
        int32_t mVerticalRange = 0;
        int32_t mVerticalOffset = 0;
        int32_t mInterval = 1;
        int32_t mMinHoverHeight = 0;
        int32_t mMaxHoverHeight = 0;
        float mHomeRadius = 0.0f;
        int32_t mMinDurationTicks = 0;
        int32_t mMaxDurationTicks = 0;
    };

    explicit RandomHoverGoal(const Settings &settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _pickTarget(Level &level, const MobActor &mob);

    bool _hasHoverHeight(Level &level, int32_t x, int32_t y, int32_t z) const;

    Settings mSettings;
    Vector3f mTarget;
    int32_t mRemainingTicks = 0;
};
