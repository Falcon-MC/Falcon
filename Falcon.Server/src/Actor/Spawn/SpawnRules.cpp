#include "Actor/Spawn/SpawnRules.h"

#include "Core/Debug/BedrockLog.h"
#include "EntityDespawnJson.h"
#include "Level/BlockStateUpgrades.h"
#include "SpawnRulesJson.h"

#include <algorithm>

namespace {
    const std::vector<std::string> UNSUPPORTED_COMPONENTS = {
            "minecraft:delay_filter", "minecraft:mob_event_filter", "minecraft:player_in_village_filter"
    };

    int32_t difficultyOf(const std::string &name, int32_t fallback) {
        if (name == "peaceful")
            return 0;
        if (name == "easy")
            return 1;
        if (name == "normal")
            return 2;
        if (name == "hard")
            return 3;
        return fallback;
    }

    std::vector<std::string> readBlocks(const json::Value &value) {
        std::vector<std::string> blocks;
        const auto add = [&blocks](const json::Value &entry) {
            if (entry.isString())
                blocks.push_back(BlockStateUpgrades::currentName(entry.mString));
            else if (entry.isObject() && entry.get("name") != nullptr)
                blocks.push_back(BlockStateUpgrades::currentName(entry.get("name")->string()));
        };

        if (value.isArray()) {
            for (const std::unique_ptr<json::Value> &entry: value.mArray)
                add(*entry);
        } else {
            add(value);
        }
        return blocks;
    }

    SpawnHerd readHerd(const json::Value &value) {
        SpawnHerd herd;
        herd.mMinSize = std::max(1, value.get("min_size") != nullptr ? value.get("min_size")->integer(1) : 1);
        herd.mMaxSize = std::max(herd.mMinSize,
                                 value.get("max_size") != nullptr ? value.get("max_size")->integer(1) : 1);
        return herd;
    }

    std::string withoutEvent(const std::string &identifier) {
        const size_t event = identifier.find('<');
        return event == std::string::npos ? identifier : identifier.substr(0, event);
    }

    SpawnCondition readCondition(const json::Value &condition) {
        SpawnCondition result;

        for (const std::string &component: UNSUPPORTED_COMPONENTS) {
            if (condition.get(component) != nullptr)
                result.mSupported = false;
        }

        result.mOnSurface = condition.get("minecraft:spawns_on_surface") != nullptr;
        result.mUnderground = condition.get("minecraft:spawns_underground") != nullptr;
        result.mUnderwater = condition.get("minecraft:spawns_underwater") != nullptr;
        result.mLava = condition.get("minecraft:spawns_lava") != nullptr;
        result.mNoBubbles = condition.get("minecraft:disallow_spawns_in_bubble") != nullptr;

        if (const json::Value *weight = condition.get("minecraft:weight"))
            result.mWeight = weight->get("default") != nullptr ? weight->get("default")->integer(0) : 0;

        if (const json::Value *herd = condition.get("minecraft:herd")) {
            if (herd->isArray()) {
                for (const std::unique_ptr<json::Value> &entry: herd->mArray)
                    result.mHerds.push_back(readHerd(*entry));
            } else {
                result.mHerds.push_back(readHerd(*herd));
            }
        }
        if (result.mHerds.empty())
            result.mHerds.push_back(SpawnHerd());

        if (const json::Value *permutations = condition.get("minecraft:permute_type")) {
            for (const std::unique_ptr<json::Value> &entry: permutations->mArray) {
                SpawnPermutation permutation;
                permutation.mWeight = entry->get("weight") != nullptr ? entry->get("weight")->integer(0) : 0;
                if (const json::Value *type = entry->get("entity_type"))
                    permutation.mIdentifier = withoutEvent(type->string());
                result.mPermutations.push_back(permutation);
            }
        }

        if (const json::Value *brightness = condition.get("minecraft:brightness_filter")) {
            result.mHasBrightness = true;
            result.mMinBrightness = brightness->get("min") != nullptr ? brightness->get("min")->integer(0) : 0;
            result.mMaxBrightness = brightness->get("max") != nullptr ? brightness->get("max")->integer(15) : 15;
            result.mAdjustForWeather = brightness->get("adjust_for_weather") != nullptr
                                       && brightness->get("adjust_for_weather")->boolean(false);
        }

        if (const json::Value *difficulty = condition.get("minecraft:difficulty_filter")) {
            if (const json::Value *minimum = difficulty->get("min"))
                result.mMinDifficulty = difficultyOf(minimum->string(), 0);
            if (const json::Value *maximum = difficulty->get("max"))
                result.mMaxDifficulty = difficultyOf(maximum->string(), 3);
        }

        if (const json::Value *height = condition.get("minecraft:height_filter")) {
            result.mHasHeight = true;
            result.mMinHeight = height->get("min") != nullptr ? height->get("min")->integer(0) : INT32_MIN;
            result.mMaxHeight = height->get("max") != nullptr ? height->get("max")->integer(0) : INT32_MAX;
        }

        if (const json::Value *distance = condition.get("minecraft:distance_filter")) {
            result.mHasDistance = true;
            result.mMinDistance = (float) (distance->get("min") != nullptr ? distance->get("min")->number(0.0) : 0.0);
            result.mMaxDistance = (float) (distance->get("max") != nullptr ? distance->get("max")->number(0.0) : 0.0);
        }

        if (const json::Value *age = condition.get("minecraft:world_age_filter"))
            result.mMinWorldAge = age->get("min") != nullptr ? (int64_t) age->get("min")->number(0.0) : 0;

        if (const json::Value *density = condition.get("minecraft:density_limit")) {
            if (const json::Value *surface = density->get("surface"))
                result.mSurfaceDensity = surface->integer(-1);
            if (const json::Value *underground = density->get("underground"))
                result.mUndergroundDensity = underground->integer(-1);
        }

        if (const json::Value *blocks = condition.get("minecraft:spawns_on_block_filter"))
            result.mOnBlocks = readBlocks(*blocks);
        if (const json::Value *blocks = condition.get("minecraft:spawns_on_block_prevented_filter"))
            result.mPreventedBlocks = readBlocks(*blocks);

        if (const json::Value *biome = condition.get("minecraft:biome_filter"))
            result.mBiomeFilter = std::shared_ptr<json::Value>(biome->clone());

        return result;
    }

    DespawnRule readDespawn(const json::Value &component) {
        DespawnRule rule;
        if (const json::Value *distance = component.get("despawn_from_distance")) {
            rule.mFromDistance = true;
            if (const json::Value *minimum = distance->get("min_distance"))
                rule.mMinDistance = (float) minimum->number(rule.mMinDistance);
            if (const json::Value *maximum = distance->get("max_distance"))
                rule.mMaxDistance = (float) maximum->number(rule.mMaxDistance);
        }
        if (const json::Value *chance = component.get("despawn_from_chance"))
            rule.mFromChance = chance->boolean(true);
        if (const json::Value *chance = component.get("min_range_random_chance"))
            rule.mRandomChance = std::max(1, chance->integer(rule.mRandomChance));
        if (const json::Value *inactivity = component.get("despawn_from_inactivity"))
            rule.mFromInactivity = inactivity->boolean(true);
        if (const json::Value *timer = component.get("min_range_inactivity_timer"))
            rule.mInactivitySeconds = std::max(0, timer->integer(rule.mInactivitySeconds));
        return rule;
    }
}

std::vector<SpawnRule> &SpawnRules::_rules() {
    static std::vector<SpawnRule> rules;
    return rules;
}

std::unordered_map<std::string, std::string> &SpawnRules::_populations() {
    static std::unordered_map<std::string, std::string> populations;
    return populations;
}

std::unordered_map<std::string, DespawnRule> &SpawnRules::_despawnRules() {
    static std::unordered_map<std::string, DespawnRule> despawnRules;
    return despawnRules;
}

void SpawnRules::initialize() {
    _rules().clear();
    _populations().clear();
    _despawnRules().clear();

    const std::unique_ptr<json::Value> rules = json::parse(FalconSpawnData::kSpawnRulesJson);
    if (rules == nullptr || !rules->isObject()) {
        LOG_WARN(LogAreaID::Server, "Could not parse the embedded spawn rules");
        return;
    }

    for (const std::string &identifier: rules->mKeys) {
        const json::Value &entry = *rules->mObject.at(identifier);

        SpawnRule rule;
        rule.mIdentifier = identifier;
        rule.mPopulation = entry.get("population_control") != nullptr ? entry.get("population_control")->string() : "";
        if (const json::Value *conditions = entry.get("conditions")) {
            for (const std::unique_ptr<json::Value> &condition: conditions->mArray)
                rule.mConditions.push_back(readCondition(*condition));
        }

        _populations()[identifier] = rule.mPopulation;
        _rules().push_back(std::move(rule));
    }

    const std::unique_ptr<json::Value> despawns = json::parse(FalconSpawnData::kEntityDespawnJson);
    if (despawns != nullptr && despawns->isObject()) {
        for (const std::string &identifier: despawns->mKeys) {
            const json::Value &component = *despawns->mObject.at(identifier);
            if (component.get("despawn_from_distance") != nullptr)
                _despawnRules()[identifier] = readDespawn(component);
        }
    }

    LOG_INFO(LogAreaID::Server, "Loaded %zu spawn rules and %zu despawn rules", _rules().size(),
             _despawnRules().size());
}

const std::vector<SpawnRule> &SpawnRules::getRules() {
    return _rules();
}

const std::string &SpawnRules::getPopulation(const std::string &identifier) {
    static const std::string none;
    const auto found = _populations().find(identifier);
    return found == _populations().end() ? none : found->second;
}

const DespawnRule *SpawnRules::getDespawnRule(const std::string &identifier) {
    const auto found = _despawnRules().find(identifier);
    return found == _despawnRules().end() ? nullptr : &found->second;
}
