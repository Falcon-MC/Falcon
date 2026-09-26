#include "Actor/AI/Goal/BehaviorGoals.h"

#include "Actor/AI/Goal/AvoidMobTypeGoal.h"
#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/AI/Goal/BreakDoorGoal.h"
#include "Actor/AI/Goal/BreedGoal.h"
#include "Actor/AI/Goal/EatBlockGoal.h"
#include "Actor/AI/Goal/FleeSunGoal.h"
#include "Actor/AI/Goal/FloatGoal.h"
#include "Actor/AI/Goal/FollowOwnerGoal.h"
#include "Actor/AI/Goal/FollowParentGoal.h"
#include "Actor/AI/Goal/GoHomeGoal.h"
#include "Actor/AI/Goal/GoalSelector.h"
#include "Actor/AI/Goal/HurtByTargetGoal.h"
#include "Actor/AI/Goal/LeapAtTargetGoal.h"
#include "Actor/AI/Goal/LookAtPlayerGoal.h"
#include "Actor/AI/Goal/MeleeAttackGoal.h"
#include "Actor/AI/Goal/MoveToBlockGoal.h"
#include "Actor/AI/Goal/MoveTowardsHomeRestrictionGoal.h"
#include "Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "Actor/AI/Goal/OpenDoorGoal.h"
#include "Actor/AI/Goal/OwnerTargetGoal.h"
#include "Actor/AI/Goal/PanicGoal.h"
#include "Actor/AI/Goal/PlaceBlockGoal.h"
#include "Actor/AI/Goal/RandomHoverGoal.h"
#include "Actor/AI/Goal/RandomLookAroundGoal.h"
#include "Actor/AI/Goal/RandomStrollGoal.h"
#include "Actor/AI/Goal/RangedAttackGoal.h"
#include "Actor/AI/Goal/StayWhileSittingGoal.h"
#include "Actor/AI/Goal/SwellGoal.h"
#include "Actor/AI/Goal/TemptGoal.h"
#include "Actor/AI/Goal/TimerFlagGoal.h"
#include "Actor/Mob/MobActor.h"
#include "Level/BlockStateUpgrades.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_set>
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

    const char *const OPEN_DOOR_ANNOTATION = "minecraft:annotation.open_door";
    const char *const BREAK_DOOR_ANNOTATION = "minecraft:annotation.break_door";
    const char *const FLEE_SUN_BEHAVIOR = "minecraft:behavior.flee_sun";
    const char *const NAVIGATION_COMPONENTS[] = {"minecraft:navigation.walk", "minecraft:navigation.generic"};
    const int32_t DOOR_GOAL_PRIORITY = 1;
    const float BREAK_DOOR_DEFAULT_SECONDS = 12.0f;

    const char *const HOME_COMPONENT = "minecraft:home";
    const char *const FLYING_NAVIGATION_COMPONENTS[] = {"minecraft:navigation.hover", "minecraft:navigation.fly"};
    const float MOVE_TO_BLOCK_DEFAULT_INTERVAL = 20.0f;
    const float MOVE_TO_BLOCK_DEFAULT_HEIGHT = 1.0f;
    const float MOVE_TO_BLOCK_DEFAULT_RADIUS = 0.5f;
    const float GO_HOME_DEFAULT_INTERVAL = 120.0f;
    const float GO_HOME_DEFAULT_RADIUS = 0.5f;
    const float HOVER_DEFAULT_XZ_DISTANCE = 10.0f;
    const float HOVER_DEFAULT_Y_DISTANCE = 7.0f;
    const float HOVER_DEFAULT_INTERVAL = 120.0f;
    const char *const TIMER_FLAG_PREFIX = "timer_flag_";
    const ActorFlag TIMER_FLAGS[] = {ActorFlag::TimerFlag1, ActorFlag::TimerFlag2, ActorFlag::TimerFlag3};
    const float TIMER_FLAG_DEFAULT_DURATION = 2.0f;
    const float TIMER_FLAG_DEFAULT_COOLDOWN = 10.0f;

    float numberOf(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    std::vector<NearestAttackableTargetGoal::Entry> targetEntries(const json::Value &component) {
        std::vector<NearestAttackableTargetGoal::Entry> entries;
        const json::Value *types = component.get("entity_types");
        if (types == nullptr)
            return entries;

        const auto add = [&entries](const json::Value &type) {
            NearestAttackableTargetGoal::Entry entry;
            if (const json::Value *filters = type.get("filters"))
                entry.mFilters = std::shared_ptr<json::Value>(filters->clone());
            entry.mMaxDistance = numberOf(type, "max_dist", 0.0f);
            entries.push_back(std::move(entry));
        };

        if (types->isArray()) {
            for (const std::unique_ptr<json::Value> &type: types->mArray)
                add(*type);
        } else {
            add(*types);
        }
        return entries;
    }

    std::shared_ptr<json::Value> firstFilters(const json::Value &component) {
        const json::Value *types = component.get("entity_types");
        if (types == nullptr)
            return nullptr;

        const json::Value *first = types->isArray() ? (types->mArray.empty() ? nullptr : types->mArray[0].get())
                                                    : types;
        const json::Value *filters = first == nullptr ? nullptr : first->get("filters");
        return filters == nullptr ? nullptr : std::shared_ptr<json::Value>(filters->clone());
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

    bool isFlagSet(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        return value != nullptr && value->boolean(false);
    }

    int32_t timerFlagIndex(const std::string &behavior) {
        const std::string prefix = TIMER_FLAG_PREFIX;
        if (behavior.size() != prefix.size() + 1 || behavior.compare(0, prefix.size(), prefix) != 0)
            return -1;

        const int32_t index = behavior.back() - '1';
        return index >= 0 && index < (int32_t) (sizeof(TIMER_FLAGS) / sizeof(TIMER_FLAGS[0])) ? index : -1;
    }

    int32_t rangeTicks(const json::Value &component, const char *key, const char *field, float fallback) {
        return std::max(0, (int32_t) std::lround(rangeValue(component, key, field, fallback) * TICKS_PER_SECOND));
    }

    std::shared_ptr<json::Value> triggerOf(const json::Value &component, const char *key) {
        const json::Value *trigger = component.get(key);
        return trigger == nullptr ? nullptr : std::shared_ptr<json::Value>(trigger->clone());
    }

    uint8_t controlFlagsOf(const json::Value &component) {
        const json::Value *flags = component.get("control_flags");
        if (flags == nullptr || !flags->isArray())
            return 0;

        uint8_t result = 0;
        for (const std::unique_ptr<json::Value> &flag: flags->mArray) {
            const std::string name = flag->string();
            if (name == "move")
                result |= (uint8_t) GoalControlFlag::Move;
            else if (name == "look")
                result |= (uint8_t) GoalControlFlag::Look;
            else if (name == "jump")
                result |= (uint8_t) GoalControlFlag::Jump;
        }
        return result;
    }

    bool canOpenDoors(const MobActor &mob) {
        if (mob.getComponent(OPEN_DOOR_ANNOTATION) != nullptr || mob.getComponent(BREAK_DOOR_ANNOTATION) != nullptr)
            return true;

        for (const char *name: NAVIGATION_COMPONENTS) {
            const json::Value *navigation = mob.getComponent(name);
            if (navigation != nullptr && isFlagSet(*navigation, "can_open_doors"))
                return true;
        }
        return false;
    }

    bool canFly(const MobActor &mob) {
        for (const char *name: FLYING_NAVIGATION_COMPONENTS) {
            if (mob.getComponent(name) != nullptr)
                return true;
        }
        return false;
    }

    std::shared_ptr<json::Value> cloneOf(const json::Value *value) {
        return value == nullptr ? nullptr : std::shared_ptr<json::Value>(value->clone());
    }

    std::unordered_set<std::string> blockNames(const json::Value *list) {
        std::unordered_set<std::string> names;
        if (list == nullptr)
            return names;

        if (list->isString()) {
            names.insert(BlockStateUpgrades::currentName(list->mString));
            return names;
        }

        for (const std::unique_ptr<json::Value> &entry: list->mArray) {
            if (entry->isString())
                names.insert(BlockStateUpgrades::currentName(entry->mString));
        }
        return names;
    }

    Vector3f vectorOf(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        if (value == nullptr || !value->isArray() || value->mArray.size() < 3)
            return Vector3f(0.0f, 0.0f, 0.0f);

        return Vector3f((float) value->mArray[0]->number(0.0), (float) value->mArray[1]->number(0.0),
                        (float) value->mArray[2]->number(0.0));
    }

    float homeRadius(const MobActor &mob) {
        const json::Value *home = mob.getComponent(HOME_COMPONENT);
        return home == nullptr ? 0.0f : numberOf(*home, "restriction_radius", 0.0f);
    }

    float randomMovementRadius(const MobActor &mob) {
        const json::Value *home = mob.getComponent(HOME_COMPONENT);
        const json::Value *type = home == nullptr ? nullptr : home->get("restriction_type");
        if (type == nullptr || type->string() == "none")
            return 0.0f;
        return homeRadius(mob);
    }

    RandomHoverGoal::Settings hoverSettings(const MobActor &mob, const json::Value &component, float speed) {
        RandomHoverGoal::Settings settings;
        settings.mSpeed = speed;
        settings.mHorizontalRange = (int32_t) numberOf(component, "xz_dist", HOVER_DEFAULT_XZ_DISTANCE);
        settings.mVerticalRange = (int32_t) numberOf(component, "y_dist", HOVER_DEFAULT_Y_DISTANCE);
        settings.mVerticalOffset = (int32_t) numberOf(component, "y_offset", 0.0f);
        settings.mInterval = (int32_t) numberOf(component, "interval", HOVER_DEFAULT_INTERVAL);
        settings.mHomeRadius = randomMovementRadius(mob);

        const json::Value *height = component.get("hover_height");
        if (height != nullptr && height->isArray() && height->mArray.size() >= 2) {
            settings.mMinHoverHeight = height->mArray[0]->integer(0);
            settings.mMaxHoverHeight = height->mArray[1]->integer(0);
        }
        return settings;
    }

    MoveToBlockGoal::Settings moveToBlockSettings(const json::Value &component, float speed) {
        MoveToBlockGoal::Settings settings;
        settings.mSpeed = speed;
        settings.mTickInterval = (int32_t) numberOf(component, "tick_interval", MOVE_TO_BLOCK_DEFAULT_INTERVAL);
        settings.mStartChance = numberOf(component, "start_chance", 1.0f);
        settings.mSearchRange = (int32_t) numberOf(component, "search_range", 0.0f);
        settings.mSearchHeight = (int32_t) numberOf(component, "search_height", MOVE_TO_BLOCK_DEFAULT_HEIGHT);
        settings.mGoalRadius = numberOf(component, "goal_radius", MOVE_TO_BLOCK_DEFAULT_RADIUS);
        settings.mStayTicks = (int32_t) std::lround(numberOf(component, "stay_duration", 0.0f) * TICKS_PER_SECOND);
        const json::Value *selection = component.get("target_selection_method");
        settings.mRandomTarget = selection != nullptr && selection->string() == "random";
        settings.mTargetOffset = vectorOf(component, "target_offset");
        settings.mTargetBlocks = blockNames(component.get("target_blocks"));
        settings.mTargetFilters = cloneOf(component.get("target_block_filters"));
        settings.mOnReach = cloneOf(component.get("on_reach"));
        settings.mOnStayCompleted = cloneOf(component.get("on_stay_completed"));
        return settings;
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

    PathNavigation &navigation = mob.getNavigation();
    navigation.setCanOpenDoors(canOpenDoors(mob));
    navigation.setAvoidSun(mob.getComponent(FLEE_SUN_BEHAVIOR) != nullptr);

    const bool flying = canFly(mob);
    navigation.setFlying(flying);
    mob.getMoveControl().setFlying(flying);

    if (mob.getComponent(HOME_COMPONENT) != nullptr && !mob.hasHome())
        mob.setHomePosition(mob.getPosition());

    if (mob.getComponent(OPEN_DOOR_ANNOTATION) != nullptr)
        selector.addGoal(DOOR_GOAL_PRIORITY, std::make_unique<OpenDoorGoal>());

    const json::Value *breakDoor = mob.getComponent(BREAK_DOOR_ANNOTATION);
    if (breakDoor != nullptr) {
        const float seconds = numberOf(*breakDoor, "break_time", BREAK_DOOR_DEFAULT_SECONDS);
        selector.addGoal(DOOR_GOAL_PRIORITY, std::make_unique<BreakDoorGoal>(secondsToTicks(seconds)));
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
        return std::make_unique<HurtByTargetGoal>(firstFilters(component));

    if (behavior == "nearest_attackable_target" || behavior == "nearest_prioritized_attackable_target")
        return std::make_unique<NearestAttackableTargetGoal>(targetRange(mob, component), targetEntries(component));

    if (behavior == "owner_hurt_by_target")
        return std::make_unique<OwnerTargetGoal>(OwnerTargetGoal::Mode::OwnerHurtBy);

    if (behavior == "owner_hurt_target")
        return std::make_unique<OwnerTargetGoal>(OwnerTargetGoal::Mode::OwnerHurt);

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

    if (behavior == "place_block")
        return PlaceBlockGoal::create(component);

    if (behavior == "move_to_block") {
        MoveToBlockGoal::Settings settings = moveToBlockSettings(component, speed);
        if (settings.mTargetBlocks.empty())
            return nullptr;
        return std::make_unique<MoveToBlockGoal>(std::move(settings));
    }

    if (behavior == "go_home")
        return std::make_unique<GoHomeGoal>(speed, (int32_t) numberOf(component, "interval", GO_HOME_DEFAULT_INTERVAL),
                                            numberOf(component, "goal_radius", GO_HOME_DEFAULT_RADIUS),
                                            cloneOf(component.get("on_home")), cloneOf(component.get("on_failed")));

    if (behavior == "random_hover")
        return std::make_unique<RandomHoverGoal>(hoverSettings(mob, component, speed));

    if (behavior == "move_towards_home_restriction")
        return std::make_unique<MoveTowardsHomeRestrictionGoal>(speed, homeRadius(mob));

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

    const int32_t timerFlag = timerFlagIndex(behavior);
    if (timerFlag >= 0) {
        return std::make_unique<TimerFlagGoal>(
                TIMER_FLAGS[timerFlag],
                rangeTicks(component, "duration_range", "min", TIMER_FLAG_DEFAULT_DURATION),
                rangeTicks(component, "duration_range", "max", TIMER_FLAG_DEFAULT_DURATION),
                rangeTicks(component, "cooldown_range", "min", TIMER_FLAG_DEFAULT_COOLDOWN),
                rangeTicks(component, "cooldown_range", "max", TIMER_FLAG_DEFAULT_COOLDOWN),
                triggerOf(component, "on_start"), triggerOf(component, "on_end"), controlFlagsOf(component));
    }

    return nullptr;
}
