#include "Actor/ActorCategory.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/Mob/MobActor.h"

#include "Actor/ActorFlags.h"
#include "Actor/ServerActor.h"

#include <unordered_map>
#include <unordered_set>

namespace {
    const std::unordered_map<std::string, ActorCategory> &categories() {
        static const std::unordered_map<std::string, ActorCategory> table = [] {
            std::unordered_map<std::string, ActorCategory> result;

            for (const char *identifier: {
                    "minecraft:blaze", "minecraft:bogged", "minecraft:breeze", "minecraft:cave_spider",
                    "minecraft:creaking", "minecraft:creeper", "minecraft:drowned", "minecraft:elder_guardian",
                    "minecraft:ender_dragon", "minecraft:endermite", "minecraft:evocation_illager",
                    "minecraft:ghast", "minecraft:guardian", "minecraft:hoglin", "minecraft:husk",
                    "minecraft:magma_cube", "minecraft:parched", "minecraft:phantom", "minecraft:piglin_brute",
                    "minecraft:pillager", "minecraft:ravager", "minecraft:shulker", "minecraft:silverfish",
                    "minecraft:skeleton", "minecraft:slime", "minecraft:spider", "minecraft:stray",
                    "minecraft:vex", "minecraft:vindicator", "minecraft:warden", "minecraft:witch",
                    "minecraft:wither", "minecraft:wither_skeleton", "minecraft:zoglin", "minecraft:zombie",
                    "minecraft:zombie_nautilus", "minecraft:zombie_villager", "minecraft:zombie_villager_v2"})
                result.emplace(identifier, ActorCategory::Hostile);

            for (const char *identifier: {
                    "minecraft:bee", "minecraft:dolphin", "minecraft:enderman", "minecraft:goat",
                    "minecraft:iron_golem", "minecraft:llama", "minecraft:nautilus", "minecraft:panda",
                    "minecraft:piglin", "minecraft:polar_bear", "minecraft:pufferfish",
                    "minecraft:trader_llama", "minecraft:wolf", "minecraft:zombie_pigman"})
                result.emplace(identifier, ActorCategory::Neutral);

            for (const char *identifier: {
                    "minecraft:allay", "minecraft:armadillo", "minecraft:axolotl", "minecraft:bat",
                    "minecraft:camel", "minecraft:camel_husk", "minecraft:cat", "minecraft:chicken",
                    "minecraft:cod", "minecraft:copper_golem", "minecraft:cow", "minecraft:donkey",
                    "minecraft:fox", "minecraft:frog", "minecraft:glow_squid", "minecraft:happy_ghast",
                    "minecraft:horse", "minecraft:mooshroom", "minecraft:mule", "minecraft:ocelot",
                    "minecraft:parrot", "minecraft:pig", "minecraft:rabbit", "minecraft:salmon",
                    "minecraft:sheep", "minecraft:skeleton_horse", "minecraft:sniffer", "minecraft:snow_golem",
                    "minecraft:squid", "minecraft:strider", "minecraft:sulfur_cube", "minecraft:tadpole",
                    "minecraft:tropicalfish", "minecraft:turtle", "minecraft:villager", "minecraft:villager_v2",
                    "minecraft:wandering_trader", "minecraft:zombie_horse"})
                result.emplace(identifier, ActorCategory::Passive);

            return result;
        }();

        return table;
    }

    const std::unordered_set<std::string> &sleepPreventers() {
        static const std::unordered_set<std::string> table = {
                "minecraft:blaze", "minecraft:bogged", "minecraft:cave_spider", "minecraft:creeper",
                "minecraft:drowned", "minecraft:elder_guardian", "minecraft:endermite",
                "minecraft:evocation_illager", "minecraft:guardian", "minecraft:husk", "minecraft:phantom",
                "minecraft:pillager", "minecraft:ravager", "minecraft:silverfish", "minecraft:skeleton",
                "minecraft:spider", "minecraft:stray", "minecraft:vex", "minecraft:vindicator", "minecraft:witch",
                "minecraft:wither", "minecraft:wither_skeleton", "minecraft:zoglin", "minecraft:zombie",
                "minecraft:zombie_villager", "minecraft:zombie_villager_v2"
        };

        return table;
    }
}

ActorCategory ActorCategories::of(const std::string &identifier) {
    const MobActor *mob = ActorClassRegistry::getPrototype(identifier);
    if (mob != nullptr)
        return mob->getCategory();

    const auto it = categories().find(identifier);
    return it == categories().end() ? ActorCategory::Other : it->second;
}

bool ActorCategories::isHostile(const std::string &identifier) {
    return of(identifier) == ActorCategory::Hostile;
}

bool ActorCategories::isNeutral(const std::string &identifier) {
    return of(identifier) == ActorCategory::Neutral;
}

bool ActorCategories::isPassive(const std::string &identifier) {
    return of(identifier) == ActorCategory::Passive;
}

bool ActorCategories::isPreventingSleep(const ServerActor &actor) {
    const std::string identifier = actor.getIdentifier();

    if (identifier == "minecraft:enderman" || identifier == "minecraft:zombie_pigman")
        return actor.getFlags().get(ActorFlag::Angry);

    if (identifier == "minecraft:piglin")
        return !actor.getFlags().get(ActorFlag::Baby);

    return sleepPreventers().count(identifier) != 0;
}
