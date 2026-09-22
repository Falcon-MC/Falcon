#include "Actor/Mob/Passive/CowActor.h"

#include "Actor/AI/Goal/RandomStrollGoal.h"
#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(CowActor, CowActor::IDENTIFIER);

namespace {
    const int32_t STROLL_PRIORITY = 6;
    const float STROLL_SPEED = 0.2f;
    const int32_t STROLL_RANGE = 12;
    const int32_t STROLL_INTERVAL = 100;
    const int32_t STROLL_WATER_RETRIES = 10;
}

void CowActor::registerGoals(GoalSelector &goalSelector) {
    goalSelector.addGoal(STROLL_PRIORITY, std::make_unique<RandomStrollGoal>(STROLL_SPEED, STROLL_RANGE,
                                                                             STROLL_INTERVAL, true,
                                                                             STROLL_WATER_RETRIES));
}
