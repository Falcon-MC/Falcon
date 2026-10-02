#include "Actor/AI/Goal/GoalSelector.h"

#include <algorithm>

void GoalSelector::addGoal(int32_t priority, std::unique_ptr<Goal> goal) {
    mGoals.push_back({priority, std::move(goal)});
    std::stable_sort(mGoals.begin(), mGoals.end(), [](const PrioritizedGoal &left, const PrioritizedGoal &right) {
        return left.mPriority < right.mPriority;
    });
}

void GoalSelector::tick(ServerNetworkHandler &owner, MobActor &mob) {
    for (PrioritizedGoal &goal: mGoals) {
        if (goal.mRunning && !goal.mGoal->canContinueToUse(owner, mob)) {
            goal.mGoal->stop(owner, mob);
            goal.mRunning = false;
        }
    }

    for (PrioritizedGoal &candidate: mGoals) {
        if (candidate.mRunning || !_canReplaceConflicts(candidate))
            continue;

        if (!candidate.mGoal->canUse(owner, mob))
            continue;

        _stopConflicts(owner, mob, candidate);
        candidate.mRunning = true;
        candidate.mGoal->start(owner, mob);
    }

    for (PrioritizedGoal &goal: mGoals) {
        if (goal.mRunning)
            goal.mGoal->tick(owner, mob);
    }
}

void GoalSelector::stopAll(ServerNetworkHandler &owner, MobActor &mob) {
    for (PrioritizedGoal &goal: mGoals) {
        if (!goal.mRunning)
            continue;

        goal.mGoal->stop(owner, mob);
        goal.mRunning = false;
    }
}

void GoalSelector::clear(ServerNetworkHandler &owner, MobActor &mob) {
    stopAll(owner, mob);
    mGoals.clear();
}

bool GoalSelector::_canReplaceConflicts(const PrioritizedGoal &candidate) const {
    const uint8_t flags = candidate.mGoal->getRequiredControlFlags();
    for (const PrioritizedGoal &running: mGoals) {
        if (!running.mRunning || (running.mGoal->getRequiredControlFlags() & flags) == 0)
            continue;

        if (running.mPriority <= candidate.mPriority)
            return false;
    }
    return true;
}

void GoalSelector::_stopConflicts(ServerNetworkHandler &owner, MobActor &mob, const PrioritizedGoal &candidate) {
    const uint8_t flags = candidate.mGoal->getRequiredControlFlags();
    for (PrioritizedGoal &running: mGoals) {
        if (!running.mRunning || (running.mGoal->getRequiredControlFlags() & flags) == 0)
            continue;

        running.mGoal->stop(owner, mob);
        running.mRunning = false;
    }
}
