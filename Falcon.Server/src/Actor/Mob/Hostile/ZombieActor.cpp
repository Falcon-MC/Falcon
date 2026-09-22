#include "Actor/Mob/Hostile/ZombieActor.h"

#include "Actor/AI/Goal/FloatGoal.h"
#include "Actor/AI/Goal/HurtByTargetGoal.h"
#include "Actor/AI/Goal/MeleeAttackGoal.h"
#include "Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "Actor/AI/Goal/RandomStrollGoal.h"
#include "Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(ZombieActor, ZombieActor::IDENTIFIER);

namespace {
    const float HAND_DAMAGE[] = {2.5f, 3.0f, 4.5f};

    const int32_t FLOAT_PRIORITY = 0;
    const int32_t HURT_BY_TARGET_PRIORITY = 1;
    const int32_t NEAREST_TARGET_PRIORITY = 2;
    const float TARGET_RANGE = 40.0f;

    const int32_t MELEE_PRIORITY = 3;
    const float MELEE_SPEED = 0.3f;
    const int32_t MELEE_COOL_DOWN = 30;
    const float MELEE_RANGE_SQUARED = 2.5f;

    const int32_t STROLL_PRIORITY = 6;
    const float STROLL_SPEED = 0.3f;
    const int32_t STROLL_RANGE = 12;
    const int32_t STROLL_INTERVAL = 100;
    const int32_t STROLL_WATER_RETRIES = 10;
}

float ZombieActor::getAttackDamage(Difficulty difficulty) const {
    if (difficulty == Difficulty::Peaceful)
        return 0.0f;
    return HAND_DAMAGE[(int) difficulty - 1];
}

void ZombieActor::registerGoals(GoalSelector &goalSelector) {
    goalSelector.addGoal(FLOAT_PRIORITY, std::make_unique<FloatGoal>());
    goalSelector.addGoal(HURT_BY_TARGET_PRIORITY, std::make_unique<HurtByTargetGoal>());
    goalSelector.addGoal(NEAREST_TARGET_PRIORITY, std::make_unique<NearestAttackableTargetGoal>(TARGET_RANGE));
    goalSelector.addGoal(MELEE_PRIORITY, std::make_unique<MeleeAttackGoal>(MELEE_SPEED, TARGET_RANGE,
                                                                           MELEE_COOL_DOWN, MELEE_RANGE_SQUARED));
    goalSelector.addGoal(STROLL_PRIORITY, std::make_unique<RandomStrollGoal>(STROLL_SPEED, STROLL_RANGE,
                                                                             STROLL_INTERVAL, true,
                                                                             STROLL_WATER_RETRIES));
}
