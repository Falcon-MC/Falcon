#include "Actor/AI/Navigation/PathOptions.h"

#include "Actor/Mob/MobActor.h"
#include "Core/Json/Json.h"

namespace {
    struct NavigationComponent {
        const char *mName;
        NavigationMode mMode;
    };

    const NavigationComponent NAVIGATION_COMPONENTS[] = {
            {"minecraft:navigation.walk", NavigationMode::Walk},
            {"minecraft:navigation.generic", NavigationMode::Walk},
            {"minecraft:navigation.climb", NavigationMode::Climb},
            {"minecraft:navigation.swim", NavigationMode::Swim},
            {"minecraft:navigation.fly", NavigationMode::Fly},
            {"minecraft:navigation.hover", NavigationMode::Fly},
            {"minecraft:navigation.float", NavigationMode::Fly}
    };

    const char *const GLIDE_MOVEMENT_COMPONENT = "minecraft:movement.glide";
    const char *const OPEN_DOOR_ANNOTATION = "minecraft:annotation.open_door";
    const char *const BREAK_DOOR_ANNOTATION = "minecraft:annotation.break_door";
    const char *const FLEE_SUN_BEHAVIOR = "minecraft:behavior.flee_sun";

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

    return options;
}
