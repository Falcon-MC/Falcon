#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>

class Level;

class CircleAroundAnchorGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        float mGoalRadius = 0.0f;
        float mMinRadius = 0.0f;
        float mMaxRadius = 0.0f;
        float mRadiusChange = 0.0f;
        float mRadiusAdjustmentChance = 0.0f;
        float mHeightAdjustmentChance = 0.0f;
        float mAngleChange = 0.0f;
        float mMinHeightOffset = 0.0f;
        float mMaxHeightOffset = 0.0f;
        float mMinHeightAboveTarget = 0.0f;
        float mMaxHeightAboveTarget = 0.0f;
    };

    explicit CircleAroundAnchorGoal(const Settings &settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    void _updateAnchor(ServerNetworkHandler &owner, const MobActor &mob);

    void _selectNext();

    static bool _isBlocked(Level &level, const Vector3f &position, int32_t offsetY);

    Settings mSettings;
    Vector3f mAnchor;
    Vector3f mMoveTarget;
    uint64_t mAnchorTargetId = 0;
    bool mHasAnchor = false;
    float mRadius = 0.0f;
    float mHeightOffset = 0.0f;
    float mAngle = 0.0f;
    bool mClockwise = true;
};
