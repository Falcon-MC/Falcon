#include "Actor/AI/Goal/GoalSelector.h"

#include <algorithm>

void GoalSelector::addGoal(int32_t priority, std::unique_ptr<Goal> goal) {
    mRunning = nullptr;
    mGoals.push_back({priority, std::move(goal)});
    std::stable_sort(mGoals.begin(), mGoals.end(), [](const PrioritizedGoal &left, const PrioritizedGoal &right) {
        return left.mPriority < right.mPriority;
    });
}

void GoalSelector::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (mRunning != nullptr && !mRunning->mGoal->canContinueToUse(owner, mob)) {
        mRunning->mGoal->stop(owner, mob);
        mRunning = nullptr;
    }

    for (PrioritizedGoal &candidate: mGoals) {
        if (&candidate == mRunning)
            break;

        if (!candidate.mGoal->canUse(owner, mob))
            continue;

        if (mRunning != nullptr)
            mRunning->mGoal->stop(owner, mob);

        mRunning = &candidate;
        mRunning->mGoal->start(owner, mob);
        break;
    }

    if (mRunning != nullptr)
        mRunning->mGoal->tick(owner, mob);
}

void GoalSelector::stopAll(ServerNetworkHandler &owner, MobActor &mob) {
    if (mRunning == nullptr)
        return;

    mRunning->mGoal->stop(owner, mob);
    mRunning = nullptr;
}
