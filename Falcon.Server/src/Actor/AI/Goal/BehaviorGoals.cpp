#include "Actor/AI/Goal/BehaviorGoals.h"

#include "Actor/AI/Goal/AdmireItemGoal.h"
#include "Actor/AI/Goal/AvoidBlockGoal.h"
#include "Actor/AI/Goal/AvoidMobTypeGoal.h"
#include "Actor/AI/Goal/BarterGoal.h"
#include "Actor/AI/Goal/BegGoal.h"
#include "Actor/AI/Goal/BehaviorItems.h"
#include "Actor/AI/Goal/BreakDoorGoal.h"
#include "Actor/AI/Goal/BreedGoal.h"
#include "Actor/AI/Goal/ChargeHeldItemGoal.h"
#include "Actor/AI/Goal/CircleAroundAnchorGoal.h"
#include "Actor/AI/Goal/EatBlockGoal.h"
#include "Actor/AI/Goal/EquipItemGoal.h"
#include "Actor/AI/Goal/FindMountGoal.h"
#include "Actor/AI/Goal/FleeSunGoal.h"
#include "Actor/AI/Goal/FloatGoal.h"
#include "Actor/AI/Goal/FollowCaravanGoal.h"
#include "Actor/AI/Goal/FollowMobGoal.h"
#include "Actor/AI/Goal/FollowOwnerGoal.h"
#include "Actor/AI/Goal/FollowParentGoal.h"
#include "Actor/AI/Goal/GoHomeGoal.h"
#include "Actor/AI/Goal/GoalSelector.h"
#include "Actor/AI/Goal/GuardianAttackGoal.h"
#include "Actor/AI/Goal/HurtByTargetGoal.h"
#include "Actor/AI/Goal/JumpToBlockGoal.h"
#include "Actor/AI/Goal/LeapAtTargetGoal.h"
#include "Actor/AI/Goal/LookAtEntityGoal.h"
#include "Actor/AI/Goal/LookAtPlayerGoal.h"
#include "Actor/AI/Goal/MeleeAttackGoal.h"
#include "Actor/AI/Goal/MountPathingGoal.h"
#include "Actor/AI/Goal/MoveToBlockGoal.h"
#include "Actor/AI/Goal/MoveToWaterGoal.h"
#include "Actor/AI/Goal/MoveTowardsHomeRestrictionGoal.h"
#include "Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "Actor/AI/Goal/OpenDoorGoal.h"
#include "Actor/AI/Goal/OwnerTargetGoal.h"
#include "Actor/AI/Goal/PanicGoal.h"
#include "Actor/AI/Goal/PickupItemsGoal.h"
#include "Actor/AI/Goal/PlaceBlockGoal.h"
#include "Actor/AI/Goal/RamAttackGoal.h"
#include "Actor/AI/Goal/RandomHoverGoal.h"
#include "Actor/AI/Goal/RandomLookAroundGoal.h"
#include "Actor/AI/Goal/RandomStrollGoal.h"
#include "Actor/AI/Goal/RandomSwimGoal.h"
#include "Actor/AI/Goal/RangedAttackGoal.h"
#include "Actor/AI/Goal/RunAroundLikeCrazyGoal.h"
#include "Actor/AI/Goal/SilverfishGoals.h"
#include "Actor/AI/Goal/SitGoal.h"
#include "Actor/AI/Goal/SlimeGoals.h"
#include "Actor/AI/Goal/SquidMovementGoal.h"
#include "Actor/AI/Goal/StompAttackGoal.h"
#include "Actor/AI/Goal/StompTurtleEggGoal.h"
#include "Actor/AI/Goal/SwellGoal.h"
#include "Actor/AI/Goal/SwimIdleGoal.h"
#include "Actor/AI/Goal/SwoopAttackGoal.h"
#include "Actor/AI/Goal/TakeBlockGoal.h"
#include "Actor/AI/Goal/TemptGoal.h"
#include "Actor/AI/Goal/TeleportToOwnerGoal.h"
#include "Actor/AI/Goal/TimerFlagGoal.h"
#include "Actor/AI/Goal/UseKineticWeaponGoal.h"
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
    const float TEMPT_DEFAULT_STOP_DISTANCE = 2.5f;
    const float FOLLOW_MOB_DEFAULT_RANGE = 0.0f;
    const float FOLLOW_MOB_DEFAULT_STOP_DISTANCE = 2.0f;
    const float FLOAT_WANDER_DEFAULT_XZ_DISTANCE = 10.0f;
    const float FLOAT_WANDER_DEFAULT_Y_DISTANCE = 7.0f;
    const float FLOAT_WANDER_DEFAULT_DURATION = 5.0f;
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
    const char *const FLYING_NAVIGATION_COMPONENTS[] = {"minecraft:navigation.hover", "minecraft:navigation.fly",
                                                        "minecraft:navigation.float"};
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
    const float SOUND_DEFAULT_INTERVAL = 3.0f;
    const float PICKUP_DEFAULT_GOAL_RADIUS = 0.5f;
    const float LOOK_ENTITY_DEFAULT_MIN_TIME = 2.0f;
    const float LOOK_ENTITY_DEFAULT_MAX_TIME = 4.0f;
    const float LOOK_ENTITY_FULL_ANGLE = 360.0f;
    const float SWIM_DEFAULT_XZ = 10.0f;
    const float SWIM_DEFAULT_Y = 7.0f;
    const float SWIM_WANDER_DEFAULT_CHANCE = 0.00833f;
    const float SWIM_WANDER_DEFAULT_LOOK_AHEAD = 5.0f;
    const int32_t SWIM_WANDER_VERTICAL_RANGE = 2;
    const float SWIM_IDLE_DEFAULT_TIME = 5.0f;
    const float SWIM_IDLE_DEFAULT_RATE = 0.1f;
    const char *const GLIDE_MOVEMENT_COMPONENT = "minecraft:movement.glide";
    const float CIRCLE_DEFAULT_GOAL_RADIUS = 0.5f;
    const float CIRCLE_DEFAULT_MIN_RADIUS = 5.0f;
    const float CIRCLE_DEFAULT_MAX_RADIUS = 15.0f;
    const float CIRCLE_DEFAULT_RADIUS_CHANGE = 1.0f;
    const float CIRCLE_DEFAULT_RADIUS_CHANCE = 0.004f;
    const float CIRCLE_DEFAULT_HEIGHT_CHANCE = 0.002857f;
    const float CIRCLE_DEFAULT_ANGLE_CHANGE = 15.0f;
    const float SWOOP_DEFAULT_DAMAGE_REACH = 0.2f;
    const float SWOOP_DEFAULT_MIN_DELAY = 10.0f;
    const float SWOOP_DEFAULT_MAX_DELAY = 20.0f;
    const int32_t JUMP_DEFAULT_SEARCH_WIDTH = 8;
    const int32_t JUMP_DEFAULT_SEARCH_HEIGHT = 10;
    const float JUMP_DEFAULT_MINIMUM_DISTANCE = 2.0f;
    const float JUMP_DEFAULT_MAX_VELOCITY = 1.5f;
    const float JUMP_DEFAULT_SCALE_FACTOR = 0.7f;
    const float JUMP_DEFAULT_MIN_COOLDOWN = 10.0f;
    const float JUMP_DEFAULT_MAX_COOLDOWN = 20.0f;
    const float BEG_DEFAULT_MIN_LOOK_TICKS = 2.0f;
    const float BEG_DEFAULT_MAX_LOOK_TICKS = 4.0f;
    const float CARAVAN_DEFAULT_ENTITY_COUNT = 1.0f;
    const float TAKE_DEFAULT_CHANCE = 0.05f;
    const float TAKE_DEFAULT_MIN_XZ = -1.0f;
    const float TAKE_DEFAULT_MAX_XZ = 1.0f;
    const float TAKE_DEFAULT_MIN_Y = 0.0f;
    const float TAKE_DEFAULT_MAX_Y = 2.0f;
    const float STOMP_DEFAULT_RANGE_MULTIPLIER = 2.0f;
    const float STOMP_DEFAULT_NO_DAMAGE_MULTIPLIER = 2.0f;

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
        if (range->isArray()) {
            if (range->mArray.empty())
                return fallback;
            const bool minimum = std::string(field) == "min";
            return (float) (minimum ? range->mArray.front() : range->mArray.back())->number(fallback);
        }
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
            const json::Value *outnumbered = type->get("check_if_outnumbered");
            entry.mCheckIfOutnumbered = outnumbered != nullptr && outnumbered->boolean(false);
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
        return mob.getComponent(GLIDE_MOVEMENT_COMPONENT) != nullptr;
    }

    float followRange(const MobActor &mob) {
        const json::Value *follow = mob.getComponent("minecraft:follow_range");
        return follow == nullptr ? TARGET_DEFAULT_RANGE : numberOf(*follow, "value", TARGET_DEFAULT_RANGE);
    }

    float attackReachSquared(const MobActor &mob) {
        return mob.getComponent("minecraft:attack") != nullptr ? MELEE_ATTACK_RANGE_SQUARED : MELEE_NO_ATTACK;
    }

    int32_t meleeCoolDown(const json::Value &component) {
        return (int32_t) std::lround(numberOf(component, "cooldown_time", 1.0f) * TICKS_PER_SECOND);
    }

    std::shared_ptr<json::Value> cloneOf(const json::Value *value) {
        return value == nullptr ? nullptr : std::shared_ptr<json::Value>(value->clone());
    }

    std::string blockName(const std::string &name) {
        const std::string itemPrefix = "minecraft:item.";
        if (name.rfind(itemPrefix, 0) == 0)
            return BlockStateUpgrades::currentName("minecraft:" + name.substr(itemPrefix.size()));
        return BlockStateUpgrades::currentName(name);
    }

    std::unordered_set<std::string> blockNames(const json::Value *list) {
        std::unordered_set<std::string> names;
        if (list == nullptr)
            return names;

        if (list->isString()) {
            names.insert(blockName(list->mString));
            return names;
        }

        for (const std::unique_ptr<json::Value> &entry: list->mArray) {
            const json::Value *name = entry->isObject() ? entry->get("name") : entry.get();
            if (name != nullptr && name->isString())
                names.insert(blockName(name->mString));
        }
        return names;
    }

    std::string stringOf(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        return value == nullptr ? std::string() : value->string();
    }

    int32_t soundIntervalTicks(const json::Value &component, bool minimum) {
        const json::Value *interval = component.get("sound_interval");
        if (interval == nullptr || !interval->isObject())
            return secondsToTicks(SOUND_DEFAULT_INTERVAL);

        const float low = numberOf(*interval, "range_min", numberOf(*interval, "min", SOUND_DEFAULT_INTERVAL));
        const float high = numberOf(*interval, "range_max", numberOf(*interval, "max", low));
        return secondsToTicks(minimum ? low : high);
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

    RandomHoverGoal::Settings floatWanderSettings(const MobActor &mob, const json::Value &component, float speed) {
        RandomHoverGoal::Settings settings;
        settings.mSpeed = speed;
        settings.mHorizontalRange = (int32_t) numberOf(component, "surface_xz_dist", FLOAT_WANDER_DEFAULT_XZ_DISTANCE);
        settings.mVerticalRange = (int32_t) numberOf(component, "surface_y_dist", FLOAT_WANDER_DEFAULT_Y_DISTANCE);
        settings.mHomeRadius = isFlagSet(component, "use_home_position_restriction") ? randomMovementRadius(mob) : 0.0f;

        float minimum = FLOAT_WANDER_DEFAULT_DURATION;
        float maximum = FLOAT_WANDER_DEFAULT_DURATION;
        const json::Value *duration = component.get("float_duration");
        if (duration != nullptr && duration->isArray() && duration->mArray.size() >= 2) {
            minimum = (float) duration->mArray[0]->number(minimum);
            maximum = (float) duration->mArray[1]->number(maximum);
        }
        settings.mMinDurationTicks = secondsToTicks(minimum);
        settings.mMaxDurationTicks = secondsToTicks(maximum);
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

    if (behavior == "melee_attack" || behavior == "melee_box_attack")
        return std::make_unique<MeleeAttackGoal>(speed, followRange(mob), meleeCoolDown(component),
                                                 attackReachSquared(mob));

    if (behavior == "stomp_attack") {
        const float stompMultiplier = numberOf(component, "stomp_range_multiplier", STOMP_DEFAULT_RANGE_MULTIPLIER);
        const float noDamageMultiplier = numberOf(component, "no_damage_range_multiplier",
                                                  STOMP_DEFAULT_NO_DAMAGE_MULTIPLIER);
        const float stompRangeSquared = attackReachSquared(mob) * stompMultiplier * stompMultiplier;
        return std::make_unique<StompAttackGoal>(speed, followRange(mob), meleeCoolDown(component), stompRangeSquared,
                                                 stompRangeSquared * noDamageMultiplier * noDamageMultiplier);
    }

    if (behavior == "guardian_attack")
        return std::make_unique<GuardianAttackGoal>();

    if (behavior == "circle_around_anchor") {
        CircleAroundAnchorGoal::Settings settings;
        settings.mSpeed = speed;
        settings.mGoalRadius = numberOf(component, "goal_radius", CIRCLE_DEFAULT_GOAL_RADIUS);
        settings.mMinRadius = rangeValue(component, "radius_range", "min", CIRCLE_DEFAULT_MIN_RADIUS);
        settings.mMaxRadius = rangeValue(component, "radius_range", "max", CIRCLE_DEFAULT_MAX_RADIUS);
        settings.mRadiusChange = numberOf(component, "radius_change", CIRCLE_DEFAULT_RADIUS_CHANGE);
        settings.mRadiusAdjustmentChance = numberOf(component, "radius_adjustment_chance",
                                                    CIRCLE_DEFAULT_RADIUS_CHANCE);
        settings.mHeightAdjustmentChance = numberOf(component, "height_adjustment_chance",
                                                    CIRCLE_DEFAULT_HEIGHT_CHANCE);
        settings.mAngleChange = numberOf(component, "angle_change", CIRCLE_DEFAULT_ANGLE_CHANGE);
        settings.mMinHeightOffset = rangeValue(component, "height_offset_range", "min", 0.0f);
        settings.mMaxHeightOffset = rangeValue(component, "height_offset_range", "max", 0.0f);
        settings.mMinHeightAboveTarget = rangeValue(component, "height_above_target_range", "min", 0.0f);
        settings.mMaxHeightAboveTarget = rangeValue(component, "height_above_target_range", "max", 0.0f);
        return std::make_unique<CircleAroundAnchorGoal>(settings);
    }

    if (behavior == "swoop_attack")
        return std::make_unique<SwoopAttackGoal>(speed, numberOf(component, "damage_reach", SWOOP_DEFAULT_DAMAGE_REACH),
                                                 secondsToTicks(rangeValue(component, "delay_range", "min",
                                                                           SWOOP_DEFAULT_MIN_DELAY)),
                                                 secondsToTicks(rangeValue(component, "delay_range", "max",
                                                                           SWOOP_DEFAULT_MAX_DELAY)));

    if (behavior == "ram_attack") {
        RamAttackGoal::Settings settings;
        settings.mRunSpeed = mob.getMovementSpeed() * numberOf(component, "run_speed", 1.0f);
        settings.mRamSpeed = mob.getMovementSpeed() * numberOf(component, "ram_speed", 1.0f);
        settings.mMinRamDistance = numberOf(component, "min_ram_distance", 0.0f);
        settings.mRamDistance = numberOf(component, "ram_distance", 0.0f);
        settings.mKnockbackForce = numberOf(component, "knockback_force", 0.0f);
        settings.mKnockbackHeight = numberOf(component, "knockback_height", 0.0f);
        settings.mMinCooldownTicks = secondsToTicks(rangeValue(component, "cooldown_range", "min", 0.0f));
        settings.mMaxCooldownTicks = secondsToTicks(rangeValue(component, "cooldown_range", "max", 0.0f));
        settings.mPreRamSound = stringOf(component, "pre_ram_sound");
        settings.mRamImpactSound = stringOf(component, "ram_impact_sound");
        settings.mOnStart = cloneOf(component.get("on_start"));
        if (settings.mRamDistance <= 0.0f)
            return nullptr;
        return std::make_unique<RamAttackGoal>(std::move(settings));
    }

    if (behavior == "jump_to_block") {
        JumpToBlockGoal::Settings settings;
        settings.mSearchWidth = (int32_t) numberOf(component, "search_width", (float) JUMP_DEFAULT_SEARCH_WIDTH);
        settings.mSearchHeight = (int32_t) numberOf(component, "search_height", (float) JUMP_DEFAULT_SEARCH_HEIGHT);
        settings.mMinimumDistance = numberOf(component, "minimum_distance", JUMP_DEFAULT_MINIMUM_DISTANCE);
        settings.mMaxVelocity = numberOf(component, "max_velocity", JUMP_DEFAULT_MAX_VELOCITY);
        settings.mScaleFactor = numberOf(component, "scale_factor", JUMP_DEFAULT_SCALE_FACTOR);
        settings.mMinCooldownTicks = secondsToTicks(rangeValue(component, "cooldown_range", "min",
                                                               JUMP_DEFAULT_MIN_COOLDOWN));
        settings.mMaxCooldownTicks = secondsToTicks(rangeValue(component, "cooldown_range", "max",
                                                               JUMP_DEFAULT_MAX_COOLDOWN));
        settings.mPreferredBlocks = blockNames(component.get("preferred_blocks"));
        settings.mPreferredBlocksChance = numberOf(component, "preferred_blocks_chance", 1.0f);
        settings.mForbiddenBlocks = blockNames(component.get("forbidden_blocks"));
        return std::make_unique<JumpToBlockGoal>(std::move(settings));
    }

    if (behavior == "silverfish_merge_with_stone")
        return std::make_unique<SilverfishMergeWithStoneGoal>();

    if (behavior == "silverfish_wake_up_friends")
        return std::make_unique<SilverfishWakeUpFriendsGoal>();

    if (behavior == "beg")
        return std::make_unique<BegGoal>(BehaviorItems(component.get("items")),
                                         numberOf(component, "look_distance", LOOK_DEFAULT_DISTANCE),
                                         (int32_t) rangeValue(component, "look_time", "min", BEG_DEFAULT_MIN_LOOK_TICKS),
                                         (int32_t) rangeValue(component, "look_time", "max",
                                                              BEG_DEFAULT_MAX_LOOK_TICKS));

    if (behavior == "follow_caravan")
        return std::make_unique<FollowCaravanGoal>(mob.getMovementSpeed(),
                                                   numberOf(component, "speed_multiplier", 1.0f),
                                                   (int32_t) numberOf(component, "entity_count",
                                                                      CARAVAN_DEFAULT_ENTITY_COUNT),
                                                   firstFilters(component));

    if (behavior == "take_block") {
        TakeBlockGoal::Settings settings;
        settings.mBlocks = blockNames(component.get("blocks"));
        settings.mChance = numberOf(component, "chance", TAKE_DEFAULT_CHANCE);
        settings.mMinXz = (int32_t) rangeValue(component, "xz_range", "min", TAKE_DEFAULT_MIN_XZ);
        settings.mMaxXz = (int32_t) rangeValue(component, "xz_range", "max", TAKE_DEFAULT_MAX_XZ);
        settings.mMinY = (int32_t) rangeValue(component, "y_range", "min", TAKE_DEFAULT_MIN_Y);
        settings.mMaxY = (int32_t) rangeValue(component, "y_range", "max", TAKE_DEFAULT_MAX_Y);
        if (settings.mBlocks.empty())
            return nullptr;
        return std::make_unique<TakeBlockGoal>(std::move(settings));
    }

    if (behavior == "swell")
        return std::make_unique<SwellGoal>();

    if (behavior == "random_look_around")
        return std::make_unique<RandomLookAroundGoal>();

    if (behavior == "tempt" || behavior == "float_tempt") {
        const float range = numberOf(component, "within_radius", 0.0f);
        return std::make_unique<TemptGoal>(speed, range > 0.0f ? range : TEMPT_DEFAULT_RANGE,
                                           BehaviorItems(component.get("items")),
                                           numberOf(component, "stop_distance", TEMPT_DEFAULT_STOP_DISTANCE));
    }

    if (behavior == "follow_mob") {
        const float range = numberOf(component, "search_range", FOLLOW_MOB_DEFAULT_RANGE);
        if (range <= 0.0f)
            return nullptr;

        return std::make_unique<FollowMobGoal>(speed, range,
                                               numberOf(component, "stop_distance", FOLLOW_MOB_DEFAULT_STOP_DISTANCE),
                                               cloneOf(component.get("filters")));
    }

    if (behavior == "float_wander")
        return std::make_unique<RandomHoverGoal>(floatWanderSettings(mob, component, speed));

    if (behavior == "run_around_like_crazy")
        return std::make_unique<RunAroundLikeCrazyGoal>(speed);

    if (behavior == "follow_parent")
        return std::make_unique<FollowParentGoal>(speed);

    if (behavior == "avoid_mob_type") {
        std::vector<AvoidMobTypeGoal::Entry> entries = avoidEntries(mob, component);
        if (entries.empty())
            return nullptr;

        AvoidMobTypeGoal::Options options;
        options.mRemoveTarget = isFlagSet(component, "remove_target");
        options.mOnEscape = cloneOf(component.get("on_escape_event"));
        options.mSound = stringOf(component, "avoid_mob_sound");
        options.mMinSoundInterval = soundIntervalTicks(component, true);
        options.mMaxSoundInterval = soundIntervalTicks(component, false);
        return std::make_unique<AvoidMobTypeGoal>(std::move(entries), std::move(options));
    }

    if (behavior == "avoid_block") {
        AvoidBlockGoal::Settings settings;
        settings.mTickInterval = (int32_t) numberOf(component, "tick_interval", 1.0f);
        settings.mSearchRange = (int32_t) numberOf(component, "search_range", 0.0f);
        settings.mSearchHeight = (int32_t) numberOf(component, "search_height", 0.0f);
        settings.mSprintSpeed = mob.getMovementSpeed() * numberOf(component, "sprint_speed_modifier", 1.0f);
        const json::Value *selection = component.get("target_selection_method");
        settings.mRandomTarget = selection != nullptr && selection->string() == "random";
        settings.mBlocks = blockNames(component.get("target_blocks"));
        settings.mSound = stringOf(component, "avoid_block_sound");
        settings.mMinSoundInterval = soundIntervalTicks(component, true);
        settings.mMaxSoundInterval = soundIntervalTicks(component, false);
        settings.mOnEscape = cloneOf(component.get("on_escape"));
        if (settings.mBlocks.empty() || settings.mSearchRange <= 0)
            return nullptr;
        return std::make_unique<AvoidBlockGoal>(std::move(settings));
    }

    if (behavior == "admire_item")
        return std::make_unique<AdmireItemGoal>(stringOf(component, "admire_item_sound"),
                                                soundIntervalTicks(component, true),
                                                soundIntervalTicks(component, false),
                                                cloneOf(component.get("on_admire_item_start")),
                                                cloneOf(component.get("on_admire_item_stop")));

    if (behavior == "pickup_items")
        return std::make_unique<PickupItemsGoal>(speed, numberOf(component, "max_dist", 0.0f),
                                                 numberOf(component, "goal_radius", PICKUP_DEFAULT_GOAL_RADIUS),
                                                 secondsToTicks(numberOf(component, "cooldown_after_being_attacked",
                                                                         0.0f)));

    if (behavior == "charge_held_item")
        return std::make_unique<ChargeHeldItemGoal>();

    if (behavior == "slime_float")
        return std::make_unique<SlimeFloatGoal>();

    if (behavior == "slime_keep_on_jumping")
        return std::make_unique<SlimeKeepOnJumpingGoal>();

    if (behavior == "slime_random_direction")
        return std::make_unique<SlimeRandomDirectionGoal>();

    if (behavior == "slime_attack")
        return std::make_unique<SlimeAttackGoal>();

    if (behavior == "squid_idle")
        return std::make_unique<SquidMovementGoal>();

    if (behavior == "mount_pathing")
        return std::make_unique<MountPathingGoal>(speed, numberOf(component, "target_dist", 0.0f),
                                                  isFlagSet(component, "track_target"));

    if (behavior == "teleport_to_owner")
        return std::make_unique<TeleportToOwnerGoal>(cloneOf(component.get("filters")));

    if (behavior == "look_at_entity") {
        LookAtEntityGoal::Settings settings;
        settings.mRange = numberOf(component, "look_distance", LOOK_DEFAULT_DISTANCE);
        settings.mProbability = numberOf(component, "probability", LOOK_DEFAULT_PROBABILITY);
        settings.mMinLookTicks = secondsToTicks(rangeValue(component, "look_time", "min", LOOK_ENTITY_DEFAULT_MIN_TIME));
        settings.mMaxLookTicks = secondsToTicks(rangeValue(component, "look_time", "max", LOOK_ENTITY_DEFAULT_MAX_TIME));
        settings.mHorizontalAngle = numberOf(component, "angle_of_view_horizontal", LOOK_ENTITY_FULL_ANGLE);
        settings.mFilters = cloneOf(component.get("filters"));
        return std::make_unique<LookAtEntityGoal>(settings);
    }

    if (behavior == "stomp_turtle_egg")
        return std::make_unique<StompTurtleEggGoal>(speed, (int32_t) numberOf(component, "search_range", 0.0f),
                                                    (int32_t) numberOf(component, "search_height", 0.0f),
                                                    numberOf(component, "goal_radius", PICKUP_DEFAULT_GOAL_RADIUS),
                                                    (int32_t) numberOf(component, "interval", 0.0f));

    if (behavior == "random_swim")
        return std::make_unique<RandomSwimGoal>(speed, (int32_t) numberOf(component, "xz_dist", SWIM_DEFAULT_XZ),
                                                (int32_t) numberOf(component, "y_dist", SWIM_DEFAULT_Y),
                                                (int32_t) numberOf(component, "interval", 0.0f));

    if (behavior == "swim_wander") {
        const float chance = numberOf(component, "interval", SWIM_WANDER_DEFAULT_CHANCE);
        return std::make_unique<RandomSwimGoal>(speed, (int32_t) numberOf(component, "look_ahead",
                                                                          SWIM_WANDER_DEFAULT_LOOK_AHEAD),
                                                SWIM_WANDER_VERTICAL_RANGE,
                                                chance > 0.0f ? (int32_t) std::lround(1.0f / chance) : 1);
    }

    if (behavior == "swim_idle")
        return std::make_unique<SwimIdleGoal>(secondsToTicks(numberOf(component, "idle_time", SWIM_IDLE_DEFAULT_TIME)),
                                              numberOf(component, "success_rate", SWIM_IDLE_DEFAULT_RATE));

    if (behavior == "move_to_water" || behavior == "move_to_land")
        return std::make_unique<MoveToWaterGoal>(speed, (int32_t) numberOf(component, "search_range", 0.0f),
                                                 (int32_t) numberOf(component, "search_height", 0.0f),
                                                 numberOf(component, "goal_radius", PICKUP_DEFAULT_GOAL_RADIUS),
                                                 behavior == "move_to_water");

    if (behavior == "barter")
        return std::make_unique<BarterGoal>();

    if (behavior == "equip_item")
        return std::make_unique<EquipItemGoal>();

    if (behavior == "use_kinetic_weapon") {
        UseKineticWeaponGoal::Settings settings;
        settings.mSpeed = speed;
        settings.mApproachDistance = numberOf(component, "approach_distance", settings.mApproachDistance);
        settings.mMinRepositionDistance = rangeValue(component, "reposition_distance", "min",
                                                     settings.mMinRepositionDistance);
        settings.mMaxRepositionDistance = rangeValue(component, "reposition_distance", "max",
                                                     settings.mMinRepositionDistance);
        settings.mMinCooldownDistance = rangeValue(component, "cooldown_distance", "min",
                                                   settings.mMinCooldownDistance);
        settings.mMaxCooldownDistance = rangeValue(component, "cooldown_distance", "max",
                                                   settings.mMinCooldownDistance);
        settings.mReachMultiplier = numberOf(component, "weapon_reach_multiplier", settings.mReachMultiplier);
        settings.mMinSpeedMultiplier = numberOf(component, "weapon_min_speed_multiplier",
                                                settings.mMinSpeedMultiplier);
        settings.mHijackMountNavigation = isFlagSet(component, "hijack_mount_navigation");
        return std::make_unique<UseKineticWeaponGoal>(settings);
    }

    if (behavior == "find_mount")
        return std::make_unique<FindMountGoal>(speed, numberOf(component, "within_radius", 0.0f),
                                               (int32_t) numberOf(component, "start_delay", 0.0f),
                                               (int32_t) numberOf(component, "max_failed_attempts", 0.0f));

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
        return std::make_unique<SitGoal>();

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

    if (behavior == "random_hover" || behavior == "random_fly")
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
