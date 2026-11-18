#include "World/Actor/AI/Navigation/PathOptions.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Storage/BlockStateUpgrades.h"
#include "Core/Json/Json.h"

namespace {
    struct NavigationComponent {
        const char *mName;
        NavigationMode mMode;
    };

    const NavigationComponent NAVIGATION_COMPONENTS[] = {{"minecraft:navigation.walk", NavigationMode::Walk},
                                                         {"minecraft:navigation.generic", NavigationMode::Walk},
                                                         {"minecraft:navigation.climb", NavigationMode::Climb},
                                                         {"minecraft:navigation.swim", NavigationMode::Swim},
                                                         {"minecraft:navigation.fly", NavigationMode::Fly},
                                                         {"minecraft:navigation.hover", NavigationMode::Fly},
                                                         {"minecraft:navigation.float", NavigationMode::Fly}};

    const char *const GLIDE_MOVEMENT_COMPONENT = "minecraft:movement.glide";
    const char *const OPEN_DOOR_ANNOTATION = "minecraft:annotation.open_door";
    const char *const BREAK_DOOR_ANNOTATION = "minecraft:annotation.break_door";
    const char *const FLEE_SUN_BEHAVIOR = "minecraft:behavior.flee_sun";
    const char *const PREFERRED_PATH_COMPONENT = "minecraft:preferred_path";
    const int32_t PREFERRED_DEFAULT_FALL = 3;

    void readFlag(const json::Value &component, const char *key, bool &flag) {
        const json::Value *value = component.get(key);
        if (value != nullptr)
            flag = value->boolean(flag);
    }

    void readAvoidedBlocks(const json::Value &component, std::vector<std::string> &blocks) {
        const json::Value *list = component.get("blocks_to_avoid");
        if (list == nullptr || !list->isArray())
            return;

        for (const std::unique_ptr<json::Value> &entry: list->mArray) {
            const json::Value *name = entry->isString() ? entry.get() : entry->get("name");
            if (name == nullptr)
                continue;

            const std::string identifier = name->string();
            blocks.push_back(identifier.find(':') == std::string::npos ? "minecraft:" + identifier : identifier);
        }
    }

    float numberIn(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    void readPreferredPath(const json::Value &component, PathOptions &options) {
        options.mHasPreferredPath = true;
        options.mDefaultBlockCost = numberIn(component, "default_block_cost", 0.0f);
        options.mJumpCost = numberIn(component, "jump_cost", 0.0f);
        options.mMaxFallBlocks = (int32_t) numberIn(component, "max_fall_blocks", (float) PREFERRED_DEFAULT_FALL);
        const json::Value *sets = component.get("preferred_path_blocks");
        if (sets == nullptr || !sets->isArray())
            return;

        for (const std::unique_ptr<json::Value> &set: sets->mArray) {
            const float cost = numberIn(*set, "cost", 0.0f);
            const json::Value *blocks = set->get("blocks");
            if (blocks == nullptr || !blocks->isArray())
                continue;
            for (const std::unique_ptr<json::Value> &block: blocks->mArray) {
                const json::Value *name = block->isString() ? block.get() : block->get("name");
                if (name == nullptr)
                    continue;
                const std::string identifier = name->string();
                // a block listed twice keeps its first cost
                options.mBlockCosts.emplace(
                        BlockStateUpgrades::currentName(identifier.find(':') == std::string::npos
                                                                ? "minecraft:" + identifier
                                                                : identifier),
                        cost);
            }
        }
    }
}

PathOptions PathOptions::read(const MobActor &mob) {
    PathOptions options;
    for (const NavigationComponent &candidate: NAVIGATION_COMPONENTS) {
        const json::Value *component = mob.getComponent(candidate.mName);
        if (component == nullptr)
            continue;

        options.mMode = candidate.mMode;
        readFlag(*component, "can_open_doors", options.mCanOpenDoors);
        readFlag(*component, "can_break_doors", options.mCanBreakDoors);
        readFlag(*component, "can_pass_doors", options.mCanPassDoors);
        readFlag(*component, "avoid_sun", options.mAvoidSun);
        readFlag(*component, "avoid_water", options.mAvoidWater);
        readFlag(*component, "avoid_damage_blocks", options.mAvoidDamageBlocks);
        readFlag(*component, "avoid_portals", options.mAvoidPortals);
        readFlag(*component, "can_path_over_water", options.mCanPathOverWater);
        readFlag(*component, "can_path_over_lava", options.mCanPathOverLava);
        readFlag(*component, "can_walk_in_lava", options.mCanWalkInLava);
        readFlag(*component, "can_swim", options.mCanSwim);
        readFlag(*component, "can_walk", options.mCanWalk);
        readFlag(*component, "can_sink", options.mCanSink);
        readFlag(*component, "can_float", options.mCanFloat);
        readFlag(*component, "can_breach", options.mCanBreach);
        readFlag(*component, "can_jump", options.mCanJump);
        readFlag(*component, "can_path_from_air", options.mCanPathFromAir);
        readFlag(*component, "is_amphibious", options.mIsAmphibious);
        readAvoidedBlocks(*component, options.mBlocksToAvoid);
        break;
    }

    if (mob.getComponent(GLIDE_MOVEMENT_COMPONENT) != nullptr)
        options.mMode = NavigationMode::Fly;

    if (mob.getComponent(OPEN_DOOR_ANNOTATION) != nullptr)
        options.mCanOpenDoors = true;

    if (mob.getComponent(BREAK_DOOR_ANNOTATION) != nullptr)
        options.mCanBreakDoors = true;

    if (mob.getComponent(FLEE_SUN_BEHAVIOR) != nullptr)
        options.mAvoidSun = true;

    if (const json::Value *preferredPath = mob.getComponent(PREFERRED_PATH_COMPONENT))
        readPreferredPath(*preferredPath, options);

    return options;
}
