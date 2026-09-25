#include "Actor/AI/Goal/BehaviorGoals.h"

#include "Actor/AI/Goal/AvoidMobTypeGoal.h"
#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/AI/Goal/BreedGoal.h"
#include "Actor/AI/Goal/EatBlockGoal.h"
#include "Actor/AI/Goal/FleeSunGoal.h"
#include "Actor/AI/Goal/FloatGoal.h"
#include "Actor/AI/Goal/FollowOwnerGoal.h"
#include "Actor/AI/Goal/FollowParentGoal.h"
#include "Actor/AI/Goal/GoalSelector.h"
#include "Actor/AI/Goal/HurtByTargetGoal.h"
#include "Actor/AI/Goal/LeapAtTargetGoal.h"
#include "Actor/AI/Goal/LookAtPlayerGoal.h"
#include "Actor/AI/Goal/MeleeAttackGoal.h"
#include "Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "Actor/AI/Goal/PanicGoal.h"
#include "Actor/AI/Goal/RandomLookAroundGoal.h"
#include "Actor/AI/Goal/RandomStrollGoal.h"
#include "Actor/AI/Goal/RangedAttackGoal.h"
#include "Actor/AI/Goal/StayWhileSittingGoal.h"
#include "Actor/AI/Goal/SwellGoal.h"
#include "Actor/AI/Goal/TemptGoal.h"
#include "Actor/Mob/MobActor.h"
#include "Level/BlockStateUpgrades.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {
    const char *const BEHAVIOR_PREFIX = "minecraft:behavior.";

    const int32_t PANIC_RANGE = 12;
    const int32_t PANIC_INTERVAL = 40;
    const int32_t PANIC_DURATION = 100;
    const int32_t WATER_RETRIES = 10;

    const float STROLL_DEFAULT_RANGE = 10.0f;
    const int32_t STROLL_DEFAULT_INTERVAL = 120;

    const float LOOK_DEFAULT_DISTANCE = 8.0f;
    const float LOOK_DEFAULT_PROBABILITY = 0.02f;
    const int32_t LOOK_PROBABILITY_SCALE = 1000;
    const int32_t LOOK_DURATION = 60;

    const float TARGET_DEFAULT_RANGE = 16.0f;
    const int32_t TICKS_PER_SECOND = 20;
    const float MELEE_ATTACK_RANGE_SQUARED = 2.5f;
    const float MELEE_NO_ATTACK = -1.0f;

    const float TEMPT_DEFAULT_RANGE = 10.0f;
    const float AVOID_DEFAULT_DISTANCE = 3.0f;
    const float AVOID_DEFAULT_SPRINT_DISTANCE = 7.0f;
    const float EAT_DEFAULT_SECONDS = 1.8f;
    const float RANGED_DEFAULT_RANGE = 15.0f;
    const float RANGED_DEFAULT_INTERVAL = 1.0f;
    const float FOLLOW_OWNER_START = 10.0f;
    const float FOLLOW_OWNER_STOP = 2.0f;

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool referencesPlayer(const json::Value &value) {
        if (value.isObject()) {
            const json::Value *test = value.get("test");
            const json::Value *expected = value.get("value");
            if (test != nullptr && test->string() == "is_family" && expected != nullptr
                && expected->string() == "player")
                return true;

            for (const auto &entry: value.mObject) {
                if (referencesPlayer(*entry.second))
                    return true;
            }
            return false;
        }

        for (const std::unique_ptr<json::Value> &entry: value.mArray) {
            if (referencesPlayer(*entry))
                return true;
        }
        return false;
    }

    float targetRange(const MobActor &mob, const json::Value &component) {
        const float radius = numberOf(component, "within_radius", 0.0f);
        if (radius > 0.0f)
            return radius;

        const json::Value *followRange = mob.getComponent("minecraft:follow_range");
        return followRange == nullptr ? TARGET_DEFAULT_RANGE : numberOf(*followRange, "value", TARGET_DEFAULT_RANGE);
    }

    float rangeValue(const json::Value &component, const char *key, const char *field, float fallback) {
        const json::Value *range = component.get(key);
        if (range == nullptr)
            return fallback;
        if (range->isObject())
            return numberOf(*range, field, fallback);
        return (float) range->number(fallback);
    }

    int32_t secondsToTicks(float seconds) {
        return std::max(1, (int32_t) std::lround(seconds * TICKS_PER_SECOND));
    }

    std::vector<AvoidMobTypeGoal::Entry> avoidEntries(const MobActor &mob, const json::Value &component) {
        std::vector<AvoidMobTypeGoal::Entry> entries;
        const json::Value *types = component.get("entity_types");
        if (types == nullptr)
            return entries;

        const float movement = mob.getMovementSpeed();
        for (const std::unique_ptr<json::Value> &type: types->mArray) {
            AvoidMobTypeGoal::Entry entry;
            if (const json::Value *filters = type->get("filters"))
                entry.mFilters = std::shared_ptr<json::Value>(filters->clone());
            entry.mMaxDistance = numberOf(*type, "max_dist", AVOID_DEFAULT_DISTANCE);
            entry.mWalkSpeed = movement * numberOf(*type, "walk_speed_multiplier", 1.0f);
            entry.mSprintSpeed = movement * numberOf(*type, "sprint_speed_multiplier", 1.0f);
            entry.mSprintDistance = numberOf(*type, "sprint_distance", AVOID_DEFAULT_SPRINT_DISTANCE);
            entries.push_back(std::move(entry));
        }
        return entries;
    }

    std::vector<std::pair<std::string, std::string>> eatPairs(const json::Value &component) {
        std::vector<std::pair<std::string, std::string>> pairs;
        const json::Value *list = component.get("eat_and_replace_block_pairs");
        if (list == nullptr)
            return pairs;

        for (const std::unique_ptr<json::Value> &pair: list->mArray) {
            const json::Value *eat = pair->get("eat_block");
            const json::Value *replace = pair->get("replace_block");
            if (eat != nullptr && replace != nullptr)
                pairs.emplace_back(BlockStateUpgrades::currentName(eat->string()),
                                   BlockStateUpgrades::currentName(replace->string()));
        }
        return pairs;
    }

    std::string chanceText(const json::Value &component) {
        const json::Value *chance = component.get("success_chance");
        if (chance == nullptr)
            return "0";
        return chance->isString() ? chance->mString : std::to_string(chance->number(0.0));
    }

    std::string eventName(const json::Value *trigger) {
        if (trigger == nullptr)
            return std::string();
        if (trigger->isString())
            return trigger->mString;
        const json::Value *event = trigger->get("event");
        return event == nullptr ? std::string() : event->string();
    }
}

void BehaviorGoals::build(MobActor &mob, GoalSelector &selector) {
    const std::string prefix = BEHAVIOR_PREFIX;

    for (const auto &entry: mob.getComponents()) {
        if (entry.first.rfind(prefix, 0) != 0 || entry.second == nullptr)
            continue;

        std::unique_ptr<Goal> goal = _create(mob, entry.first.substr(prefix.size()), *entry.second);
        if (goal != nullptr)
            selector.addGoal((int32_t) numberOf(*entry.second, "priority", 0.0f), std::move(goal));
    }
}

std::unique_ptr<Goal> BehaviorGoals::_create(const MobActor &mob, const std::string &behavior,
                                             const json::Value &component) {
    const float speed = mob.getMovementSpeed() * numberOf(component, "speed_multiplier", 1.0f);

    if (behavior == "float")
        return std::make_unique<FloatGoal>();

    if (behavior == "panic")
        return std::make_unique<PanicGoal>(speed, PANIC_RANGE, PANIC_INTERVAL, PANIC_DURATION, true, WATER_RETRIES);

    if (behavior == "random_stroll") {
        const int32_t range = (int32_t) numberOf(component, "xz_dist", STROLL_DEFAULT_RANGE);
        const int32_t interval = (int32_t) numberOf(component, "interval", (float) STROLL_DEFAULT_INTERVAL);
        return std::make_unique<RandomStrollGoal>(speed, range, interval, true, WATER_RETRIES);
    }

    if (behavior == "look_at_player") {
        const float distance = numberOf(component, "look_distance", LOOK_DEFAULT_DISTANCE);
        const float probability = numberOf(component, "probability", LOOK_DEFAULT_PROBABILITY);
        return std::make_unique<LookAtPlayerGoal>(distance, (int32_t) std::lround(probability * LOOK_PROBABILITY_SCALE),
                                                  LOOK_PROBABILITY_SCALE, LOOK_DURATION, 1);
    }

    if (behavior == "hurt_by_target")
        return std::make_unique<HurtByTargetGoal>();

    if (behavior == "nearest_attackable_target") {
        const json::Value *types = component.get("entity_types");
        if (types == nullptr || !referencesPlayer(*types))
            return nullptr;
        return std::make_unique<NearestAttackableTargetGoal>(targetRange(mob, component));
    }

    if (behavior == "melee_attack" || behavior == "melee_box_attack") {
        const int32_t coolDown = (int32_t) std::lround(numberOf(component, "cooldown_time", 1.0f) * TICKS_PER_SECOND);
        const float reach = mob.getComponent("minecraft:attack") != nullptr ? MELEE_ATTACK_RANGE_SQUARED
                                                                           : MELEE_NO_ATTACK;
        const json::Value *follow = mob.getComponent("minecraft:follow_range");
        const float range = follow == nullptr ? TARGET_DEFAULT_RANGE : numberOf(*follow, "value", TARGET_DEFAULT_RANGE);
        return std::make_unique<MeleeAttackGoal>(speed, range, coolDown, reach);
    }

    if (behavior == "swell")
        return std::make_unique<SwellGoal>();

    if (behavior == "random_look_around")
        return std::make_unique<RandomLookAroundGoal>();

    if (behavior == "tempt") {
        const float range = numberOf(component, "within_radius", 0.0f);
        return std::make_unique<TemptGoal>(speed, range > 0.0f ? range : TEMPT_DEFAULT_RANGE,
                                           BehaviorItems(component.get("items")));
    }

    if (behavior == "follow_parent")
        return std::make_unique<FollowParentGoal>(speed);

    if (behavior == "avoid_mob_type") {
        std::vector<AvoidMobTypeGoal::Entry> entries = avoidEntries(mob, component);
        if (entries.empty())
            return nullptr;
        return std::make_unique<AvoidMobTypeGoal>(std::move(entries));
    }

    if (behavior == "leap_at_target") {
        const json::Value *onGround = component.get("must_be_on_ground");
        return std::make_unique<LeapAtTargetGoal>(numberOf(component, "yd", 0.0f),
                                                  onGround == nullptr || onGround->boolean(true));
    }

    if (behavior == "flee_sun")
        return std::make_unique<FleeSunGoal>(speed);

    if (behavior == "breed")
        return std::make_unique<BreedGoal>(speed);

    if (behavior == "follow_owner")
        return std::make_unique<FollowOwnerGoal>(speed, numberOf(component, "start_distance", FOLLOW_OWNER_START),
                                                 numberOf(component, "stop_distance", FOLLOW_OWNER_STOP));

    if (behavior == "stay_while_sitting")
        return std::make_unique<StayWhileSittingGoal>();

    if (behavior == "eat_block") {
        std::vector<std::pair<std::string, std::string>> pairs = eatPairs(component);
        if (pairs.empty())
            return nullptr;
        return std::make_unique<EatBlockGoal>(std::move(pairs), chanceText(component),
                                              secondsToTicks(numberOf(component, "time_until_eat",
                                                                      EAT_DEFAULT_SECONDS)),
                                              eventName(component.get("on_eat")));
    }

    if (behavior == "ranged_attack") {
        const json::Value *shooter = mob.getComponent("minecraft:shooter");
        const json::Value *projectile = shooter == nullptr ? nullptr : shooter->get("def");
        if (projectile == nullptr)
            return nullptr;

        const float range = component.get("attack_radius") != nullptr
                            ? numberOf(component, "attack_radius", RANGED_DEFAULT_RANGE)
                            : rangeValue(component, "attack_range", "max", RANGED_DEFAULT_RANGE);
        const float minimum = component.get("attack_interval_min") != nullptr
                              ? numberOf(component, "attack_interval_min", RANGED_DEFAULT_INTERVAL)
                              : rangeValue(component, "attack_interval", "min", RANGED_DEFAULT_INTERVAL);
        const float maximum = component.get("attack_interval_max") != nullptr
                              ? numberOf(component, "attack_interval_max", minimum)
                              : rangeValue(component, "attack_interval", "max", minimum);
        return std::make_unique<RangedAttackGoal>(speed, range, secondsToTicks(minimum), secondsToTicks(maximum),
                                                  projectile->string());
    }

    return nullptr;
}
