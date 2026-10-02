#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

class BlockState;

class MoveToBlockGoal : public Goal {
public:
    struct Settings {
        float mSpeed = 0.0f;
        int32_t mTickInterval = 1;
        float mStartChance = 1.0f;
        int32_t mSearchRange = 0;
        int32_t mSearchHeight = 0;
        float mGoalRadius = 0.0f;
        int32_t mStayTicks = 0;
        bool mRandomTarget = false;
        Vector3f mTargetOffset;
        std::unordered_set<std::string> mTargetBlocks;
        std::shared_ptr<json::Value> mTargetFilters;
        std::shared_ptr<json::Value> mOnReach;
        std::shared_ptr<json::Value> mOnStayCompleted;
    };

    explicit MoveToBlockGoal(Settings settings);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

    void start(ServerNetworkHandler &owner, MobActor &mob) override;

    void stop(ServerNetworkHandler &owner, MobActor &mob) override;

    void tick(ServerNetworkHandler &owner, MobActor &mob) override;

private:
    bool _findTarget(ServerNetworkHandler &owner, MobActor &mob);

    bool _isTargetBlock(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position,
                        const BlockState &state) const;

    Vector3f _targetPoint() const;

    bool _isReached(const MobActor &mob) const;

    void _fire(ServerNetworkHandler &owner, MobActor &mob, const json::Value *triggers) const;

    Settings mSettings;
    Vector3i mTarget;
    bool mReached = false;
    bool mFinished = false;
    int32_t mStayTicks = 0;
    int32_t mSearchCooldown = 0;
};
