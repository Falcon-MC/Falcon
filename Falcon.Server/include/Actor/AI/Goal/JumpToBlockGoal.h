#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <string>
#include <unordered_set>

class Level;

class JumpToBlockGoal : public Goal {
public:
    struct Settings {
        int32_t mSearchWidth = 0;
        int32_t mSearchHeight = 0;
        float mMinimumDistance = 0.0f;
        float mMaxVelocity = 0.0f;
        float mScaleFactor = 1.0f;
        int32_t mMinCooldownTicks = 0;
        int32_t mMaxCooldownTicks = 0;
        std::unordered_set<std::string> mPreferredBlocks;
        float mPreferredBlocksChance = 0.0f;
        std::unordered_set<std::string> mForbiddenBlocks;
    };

    explicit JumpToBlockGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _findTarget(Level &level, const MobActor &mob, bool preferredOnly);

    bool _isLandingSpot(Level &level, const MobActor &mob, int32_t x, int32_t y, int32_t z, bool preferredOnly) const;

    bool _computeMotion(Level &level, const MobActor &mob, const Vector3f &target, Vector3f &motion) const;

    bool _isTrajectoryClear(Level &level, const MobActor &mob, Vector3f motion, float horizontalDistance) const;

    Settings mSettings;
    Vector3f mTarget;
    Vector3f mMotion;
    int64_t mNextJumpTick = 0;
};
