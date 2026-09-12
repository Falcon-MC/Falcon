#include "Actor/MobLootTable.h"

#include <algorithm>
#include <random>
#include <unordered_map>

namespace MobLootTable {

namespace {

constexpr uint32_t kLootSeed = 0x9E3779B9u;
constexpr float kLootingChancePerLevel = 0.01f;

struct LootEntry {
    const char *mItemIdentifier;
    const char *mBurntItemIdentifier;
    int32_t mMinCount;
    int32_t mMaxCount;
    float mChance;
    float mLootingChancePerLevel = kLootingChancePerLevel;
};

std::mt19937 &getRandom() {
    static std::mt19937 random(kLootSeed);
    return random;
}

float nextChanceRoll() {
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(getRandom());
}

int32_t nextCount(int32_t minCount, int32_t maxCount) {
    if (minCount >= maxCount)
        return minCount;

    std::uniform_int_distribution<int32_t> distribution(minCount, maxCount);
    return distribution(getRandom());
}

const std::unordered_map<std::string, std::vector<LootEntry>> &getTable() {
    static const std::unordered_map<std::string, std::vector<LootEntry>> kTable = {
        {"minecraft:cow", {
            {"minecraft:beef", "minecraft:cooked_beef", 1, 3, 1.0f},
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:mooshroom", {
            {"minecraft:beef", "minecraft:cooked_beef", 1, 3, 1.0f},
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:pig", {
            {"minecraft:porkchop", "minecraft:cooked_porkchop", 1, 3, 1.0f}
        }},
        {"minecraft:chicken", {
            {"minecraft:chicken", "minecraft:cooked_chicken", 1, 1, 1.0f},
            {"minecraft:feather", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:sheep", {
            {"minecraft:mutton", "minecraft:cooked_mutton", 1, 2, 1.0f},
            {"minecraft:white_wool", nullptr, 1, 1, 1.0f}
        }},
        {"minecraft:rabbit", {
            {"minecraft:rabbit_hide", nullptr, 0, 1, 0.5f},
            {"minecraft:rabbit", "minecraft:cooked_rabbit", 0, 1, 0.5f},
            {"minecraft:rabbit_foot", nullptr, 1, 1, 0.1f, 0.03f}
        }},
        {"minecraft:cod", {
            {"minecraft:cod", "minecraft:cooked_cod", 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
        }},
        {"minecraft:salmon", {
            {"minecraft:salmon", "minecraft:cooked_salmon", 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
        }},
        {"minecraft:tropicalfish", {
            {"minecraft:tropical_fish", nullptr, 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
        }},
        {"minecraft:pufferfish", {
            {"minecraft:pufferfish", nullptr, 1, 1, 1.0f},
            {"minecraft:bone", nullptr, 1, 2, 0.25f}
        }},
        {"minecraft:dolphin", {
            {"minecraft:cod", "minecraft:cooked_cod", 0, 1, 0.5f}
        }},
        {"minecraft:polar_bear", {
            {"minecraft:cod", "minecraft:cooked_cod", 0, 2, 0.6667f},
            {"minecraft:salmon", "minecraft:cooked_salmon", 0, 2, 0.6667f}
        }},
        {"minecraft:squid", {
            {"minecraft:ink_sac", nullptr, 1, 3, 1.0f}
        }},
        {"minecraft:glow_squid", {
            {"minecraft:glow_ink_sac", nullptr, 1, 3, 1.0f}
        }},
        {"minecraft:cat", {
            {"minecraft:string", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:panda", {
            {"minecraft:bamboo", nullptr, 0, 3, 1.0f}
        }},
        {"minecraft:parrot", {
            {"minecraft:feather", nullptr, 1, 2, 1.0f}
        }},
        {"minecraft:turtle", {
            {"minecraft:seagrass", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:nautilus", {
            {"minecraft:nautilus_shell", nullptr, 1, 1, 0.05f}
        }},
        {"minecraft:zombie_nautilus", {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
        }},
        {"minecraft:horse", {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:donkey", {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:mule", {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:llama", {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:trader_llama", {
            {"minecraft:leather", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:skeleton_horse", {
            {"minecraft:bone", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:zombie_horse", {
            {"minecraft:rotten_flesh", nullptr, 2, 3, 1.0f}
        }},
        {"minecraft:camel_husk", {
            {"minecraft:rotten_flesh", nullptr, 2, 3, 0.6667f}
        }},
        {"minecraft:strider", {
            {"minecraft:string", nullptr, 2, 5, 1.0f}
        }},
        {"minecraft:hoglin", {
            {"minecraft:porkchop", "minecraft:cooked_porkchop", 2, 4, 1.0f},
            {"minecraft:leather", nullptr, 0, 1, 0.5f}
        }},
        {"minecraft:zoglin", {
            {"minecraft:rotten_flesh", nullptr, 1, 3, 1.0f}
        }},
        {"minecraft:zombie", {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
        }},
        {"minecraft:husk", {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
        }},
        {"minecraft:zombie_villager", {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
        }},
        {"minecraft:zombie_villager_v2", {
            {"minecraft:rotten_flesh", nullptr, 0, 3, 1.0f}
        }},
        {"minecraft:drowned", {
            {"minecraft:rotten_flesh", nullptr, 1, 1, 1.0f}
        }},
        {"minecraft:zombie_pigman", {
            {"minecraft:rotten_flesh", nullptr, 0, 1, 1.0f},
            {"minecraft:gold_nugget", nullptr, 0, 1, 1.0f}
        }},
        {"minecraft:skeleton", {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:arrow", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:bogged", {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:arrow", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:parched", {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:arrow", nullptr, 0, 2, 1.0f},
            {"minecraft:bow", nullptr, 1, 1, 0.08f, 0.05f}
        }},
        {"minecraft:stray", {
            {"minecraft:bone", nullptr, 1, 2, 0.66f},
            {"minecraft:arrow", nullptr, 1, 2, 0.55f}
        }},
        {"minecraft:wither_skeleton", {
            {"minecraft:bone", nullptr, 0, 2, 1.0f},
            {"minecraft:coal", nullptr, 1, 1, 0.3333f},
            {"minecraft:stone_sword", nullptr, 1, 1, 0.085f, 0.05f},
            {"minecraft:wither_skeleton_skull", nullptr, 1, 1, 0.025f, 0.02f}
        }},
        {"minecraft:creeper", {
            {"minecraft:gunpowder", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:spider", {
            {"minecraft:string", nullptr, 1, 2, 0.7f},
            {"minecraft:spider_eye", nullptr, 1, 1, 0.5f}
        }},
        {"minecraft:cave_spider", {
            {"minecraft:string", nullptr, 0, 2, 1.0f},
            {"minecraft:spider_eye", nullptr, 1, 1, 0.5f}
        }},
        {"minecraft:enderman", {
            {"minecraft:ender_pearl", nullptr, 1, 1, 0.5f}
        }},
        {"minecraft:blaze", {
            {"minecraft:blaze_rod", nullptr, 1, 1, 0.5f}
        }},
        {"minecraft:breeze", {
            {"minecraft:breeze_rod", nullptr, 1, 2, 1.0f}
        }},
        {"minecraft:slime", {
            {"minecraft:slime_ball", nullptr, 1, 2, 1.0f}
        }},
        {"minecraft:magma_cube", {
            {"minecraft:magma_cream", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:ghast", {
            {"minecraft:ghast_tear", nullptr, 0, 1, 0.5f},
            {"minecraft:gunpowder", nullptr, 0, 2, 0.6667f}
        }},
        {"minecraft:phantom", {
            {"minecraft:phantom_membrane", nullptr, 0, 1, 0.5f}
        }},
        {"minecraft:shulker", {
            {"minecraft:shulker_shell", nullptr, 0, 1, 0.5f}
        }},
        {"minecraft:witch", {
            {"minecraft:redstone", nullptr, 4, 8, 1.0f},
            {"minecraft:stick", nullptr, 0, 6, 0.3349f},
            {"minecraft:spider_eye", nullptr, 0, 6, 0.1787f},
            {"minecraft:glowstone_dust", nullptr, 0, 6, 0.1787f},
            {"minecraft:gunpowder", nullptr, 0, 6, 0.1787f},
            {"minecraft:sugar", nullptr, 0, 6, 0.1787f},
            {"minecraft:glass_bottle", nullptr, 0, 6, 0.1787f}
        }},
        {"minecraft:guardian", {
            {"minecraft:prismarine_shard", nullptr, 0, 2, 1.0f},
            {"minecraft:cod", "minecraft:cooked_cod", 1, 1, 0.025f},
            {"minecraft:prismarine_crystals", nullptr, 0, 1, 0.3333f}
        }},
        {"minecraft:elder_guardian", {
            {"minecraft:prismarine_shard", nullptr, 0, 2, 1.0f},
            {"minecraft:wet_sponge", nullptr, 1, 1, 1.0f},
            {"minecraft:cod", "minecraft:cooked_cod", 1, 1, 0.025f},
            {"minecraft:prismarine_crystals", nullptr, 0, 1, 0.3333f}
        }},
        {"minecraft:iron_golem", {
            {"minecraft:iron_ingot", nullptr, 3, 5, 1.0f},
            {"minecraft:poppy", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:snow_golem", {
            {"minecraft:snowball", nullptr, 0, 15, 0.6667f}
        }},
        {"minecraft:copper_golem", {
            {"minecraft:copper_ingot", nullptr, 1, 3, 1.0f}
        }},
        {"minecraft:evocation_illager", {
            {"minecraft:totem_of_undying", nullptr, 1, 1, 1.0f},
            {"minecraft:emerald", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:vindicator", {
            {"minecraft:iron_axe", nullptr, 1, 1, 1.0f},
            {"minecraft:emerald", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:pillager", {
            {"minecraft:arrow", nullptr, 0, 2, 1.0f}
        }},
        {"minecraft:ravager", {
            {"minecraft:saddle", nullptr, 1, 1, 1.0f}
        }},
        {"minecraft:wither", {
            {"minecraft:nether_star", nullptr, 1, 1, 1.0f}
        }}
    };

    return kTable;
}

}

std::vector<MobDrop> getMobDrops(const std::string &identifier, bool onFire, int32_t lootingLevel) {
    std::vector<MobDrop> drops;

    const std::unordered_map<std::string, std::vector<LootEntry>> &table = getTable();
    const auto found = table.find(identifier);
    if (found == table.end())
        return drops;

    const int32_t looting = std::max(lootingLevel, 0);

    for (const LootEntry &entry: found->second) {
        if (entry.mChance < 1.0f) {
            const float chance = std::min(1.0f, entry.mChance + entry.mLootingChancePerLevel * (float) looting);
            if (nextChanceRoll() >= chance)
                continue;
        }

        const int32_t maxCount = entry.mMaxCount > entry.mMinCount ? entry.mMaxCount + looting : entry.mMaxCount;
        const int32_t count = nextCount(entry.mMinCount, maxCount);
        if (count <= 0)
            continue;

        const char *itemIdentifier = entry.mItemIdentifier;
        if (onFire && entry.mBurntItemIdentifier != nullptr)
            itemIdentifier = entry.mBurntItemIdentifier;

        MobDrop drop;
        drop.mItemIdentifier = itemIdentifier;
        drop.mCount = count;
        drops.push_back(drop);
    }

    return drops;
}

bool hasMobDrops(const std::string &identifier) {
    const std::unordered_map<std::string, std::vector<LootEntry>> &table = getTable();
    return table.find(identifier) != table.end();
}

}
