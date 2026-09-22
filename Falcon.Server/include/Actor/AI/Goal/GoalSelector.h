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
    };

    std::vector<PrioritizedGoal> mGoals;
    PrioritizedGoal *mRunning = nullptr;
};
