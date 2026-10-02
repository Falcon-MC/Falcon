#pragma once

#include "Actor/AI/Goal/Goal.h"
#include "Core/Json/Json.h"

#include <memory>
#include <string>

class GoalSelector;
class MobActor;

class BehaviorGoals {
public:
    static void build(MobActor &mob, GoalSelector &selector);

private:
    static std::unique_ptr<Goal> _create(const MobActor &mob, const std::string &behavior,
                                         const json::Value &component);
};
