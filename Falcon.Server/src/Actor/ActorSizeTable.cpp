#include "Actor/ActorSizeTable.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/Mob/MobActor.h"

#include <unordered_map>

namespace ActorSizeTable {

namespace {

constexpr float kDefaultWidth = 0.6f;
constexpr float kDefaultHeight = 1.8f;

struct SizeEntry {
    const char *mIdentifier;
    float mWidth;
    float mHeight;
};

const SizeEntry kSizes[] = {
    {"minecraft:npc", 0.6f, 2.1f},
    {"minecraft:armor_stand", 0.5f, 1.975f},
    {"minecraft:item", 0.25f, 0.25f},
    {"minecraft:xp_orb", 0.25f, 0.25f},
    {"minecraft:xp_bottle", 0.25f, 0.25f},
    {"minecraft:tnt", 0.98f, 0.98f},
    {"minecraft:falling_block", 0.98f, 0.98f},
    {"minecraft:ender_crystal", 0.98f, 0.98f},
    {"minecraft:fireworks_rocket", 0.25f, 0.25f},
    {"minecraft:eye_of_ender_signal", 0.25f, 0.25f},
    {"minecraft:area_effect_cloud", 3.0f, 0.5f},
    {"minecraft:arrow", 0.1f, 0.1f},
    {"minecraft:thrown_trident", 0.1f, 0.1f},
    {"minecraft:fishing_hook", 0.1f, 0.1f},
    {"minecraft:snowball", 0.25f, 0.25f},
    {"minecraft:egg", 0.25f, 0.25f},
    {"minecraft:ender_pearl", 0.25f, 0.25f},
    {"minecraft:splash_potion", 0.25f, 0.25f},
    {"minecraft:lingering_potion", 0.25f, 0.25f},
    {"minecraft:wither_skull", 0.25f, 0.25f},
    {"minecraft:wither_skull_dangerous", 0.25f, 0.25f},
    {"minecraft:fireball", 0.31f, 0.31f},
    {"minecraft:small_fireball", 0.3125f, 0.3125f},
    {"minecraft:dragon_fireball", 0.3125f, 0.3125f},
    {"minecraft:shulker_bullet", 0.3125f, 0.3125f},
    {"minecraft:wind_charge_projectile", 0.3125f, 0.3125f},
    {"minecraft:breeze_wind_charge_projectile", 0.3125f, 0.3125f},
    {"minecraft:llama_spit", 0.9f, 1.87f},
    {"minecraft:evocation_fang", 1.0f, 0.8f},
    {"minecraft:boat", 1.3f, 0.5f},
    {"minecraft:chest_boat", 1.3f, 0.5f},
    {"minecraft:minecart", 0.98f, 0.7f},
    {"minecraft:hopper_minecart", 0.98f, 0.7f},
    {"minecraft:tnt_minecart", 0.98f, 0.7f},
    {"minecraft:chest_minecart", 0.98f, 0.7f},
    {"minecraft:command_block_minecart", 0.98f, 0.7f}
};

const std::unordered_map<std::string, ActorSize> &getTable() {
    static const std::unordered_map<std::string, ActorSize> kTable = [] {
        std::unordered_map<std::string, ActorSize> table;
        table.reserve(sizeof(kSizes) / sizeof(kSizes[0]));
        for (const SizeEntry &entry : kSizes) {
            table.emplace(entry.mIdentifier, ActorSize{entry.mWidth, entry.mHeight});
        }
        return table;
    }();

    return kTable;
}

}

ActorSize getSize(const std::string &identifier) {
    const MobActor *mob = ActorClassRegistry::getPrototype(identifier);
    if (mob != nullptr)
        return mob->getSize();

    const std::unordered_map<std::string, ActorSize> &table = getTable();
    auto found = table.find(identifier);
    if (found == table.end()) {
        return ActorSize{kDefaultWidth, kDefaultHeight};
    }

    return found->second;
}

}
