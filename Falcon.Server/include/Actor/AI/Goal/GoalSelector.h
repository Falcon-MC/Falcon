#pragma once

#include "Actor/AI/Goal/Goal.h"

#include <cstdint>
#include <memory>
#include <vector>

class GoalSelector {
public:
    void addGoal(int32_t priority, std::unique_ptr<Goal> goal);

    bool isEmpty() const {
        return mGoals.empty();
    }

    void tick(ServerNetworkHandler &owner, MobActor &mob);

    void stopAll(ServerNetworkHandler &owner, MobActor &mob);

private:
    struct PrioritizedGoal {
        int32_t mPriority;
        std::unique_ptr<Goal> mGoal;
        bool mRunning = false;
    };

    bool _canReplaceConflicts(const PrioritizedGoal &candidate) const;

    void _stopConflicts(ServerNetworkHandler &owner, MobActor &mob, const PrioritizedGoal &candidate);

    std::vector<PrioritizedGoal> mGoals;
};
