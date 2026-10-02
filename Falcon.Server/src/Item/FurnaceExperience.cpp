#include "Item/FurnaceExperience.h"

#include "Item/Loot/LegacyItemMapper.h"

#include <cstdint>
#include <unordered_map>

namespace {
    struct LegacyExperience {
        const char *mIdentifier;
        int32_t mData;
        float mExperience;
    };

    const LegacyExperience LEGACY_EXPERIENCE[] = {
        {"minecraft:ancient_debris", 0, 2.0f},
        {"minecraft:basalt", 0, 0.1f},
        {"minecraft:beef", 0, 0.35f},
        {"minecraft:cactus", 0, 0.2f},
        {"minecraft:chicken", 0, 0.35f},
        {"minecraft:chorus_fruit", 0, 0.1f},
        {"minecraft:clay", 0, 0.35f},
        {"minecraft:clay_ball", 0, 0.3f},
        {"minecraft:coal_ore", 0, 0.1f},
        {"minecraft:cobbled_deepslate", 0, 0.1f},
        {"minecraft:cobblestone", 0, 0.1f},
        {"minecraft:cod", 0, 0.35f},
        {"minecraft:copper_ore", 0, 0.7f},
        {"minecraft:deepslate_bricks", 0, 0.1f},
        {"minecraft:deepslate_coal_ore", 0, 0.1f},
        {"minecraft:deepslate_copper_ore", 0, 0.7f},
        {"minecraft:deepslate_diamond_ore", 0, 1.0f},
        {"minecraft:deepslate_emerald_ore", 0, 1.0f},
        {"minecraft:deepslate_gold_ore", 0, 1.0f},
        {"minecraft:deepslate_iron_ore", 0, 0.7f},
        {"minecraft:deepslate_lapis_ore", 0, 0.2f},
        {"minecraft:deepslate_redstone_ore", 0, 0.7f},
        {"minecraft:diamond_ore", 0, 1.0f},
        {"minecraft:emerald_ore", 0, 1.0f},
        {"minecraft:gold_ore", 0, 1.0f},
        {"minecraft:golden_axe", 0, 0.1f},
        {"minecraft:golden_boots", 0, 0.1f},
        {"minecraft:golden_chestplate", 0, 0.1f},
        {"minecraft:golden_helmet", 0, 0.1f},
        {"minecraft:golden_hoe", 0, 0.1f},
        {"minecraft:golden_horse_armor", 0, 0.1f},
        {"minecraft:golden_leggings", 0, 0.1f},
        {"minecraft:golden_pickaxe", 0, 0.1f},
        {"minecraft:golden_shovel", 0, 0.1f},
        {"minecraft:golden_sword", 0, 0.1f},
        {"minecraft:iron_axe", 0, 0.1f},
        {"minecraft:iron_boots", 0, 0.1f},
        {"minecraft:iron_chestplate", 0, 0.1f},
        {"minecraft:iron_helmet", 0, 0.1f},
        {"minecraft:iron_hoe", 0, 0.1f},
        {"minecraft:iron_horse_armor", 0, 0.1f},
        {"minecraft:iron_leggings", 0, 0.1f},
        {"minecraft:iron_ore", 0, 0.7f},
        {"minecraft:iron_pickaxe", 0, 0.1f},
        {"minecraft:iron_shovel", 0, 0.1f},
        {"minecraft:iron_sword", 0, 0.1f},
        {"minecraft:kelp", 0, 0.1f},
        {"minecraft:lapis_ore", 0, 0.2f},
        {"minecraft:log2", 0, 0.15f},
        {"minecraft:log2", 1, 0.15f},
        {"minecraft:log", 0, 0.15f},
        {"minecraft:log", 1, 0.15f},
        {"minecraft:log", 2, 0.15f},
        {"minecraft:log", 3, 0.15f},
        {"minecraft:mangrove_log", 0, 0.15f},
        {"minecraft:mangrove_wood", 0, 0.15f},
        {"minecraft:mutton", 0, 0.35f},
        {"minecraft:nether_brick", 0, 0.1f},
        {"minecraft:nether_gold_ore", 0, 1.0f},
        {"minecraft:netherrack", 0, 0.1f},
        {"minecraft:polished_blackstone_bricks", 0, 0.1f},
        {"minecraft:porkchop", 0, 0.35f},
        {"minecraft:potato", 0, 0.35f},
        {"minecraft:quartz_block", 0, 0.1f},
        {"minecraft:quartz_ore", 0, 0.2f},
        {"minecraft:rabbit", 0, 0.35f},
        {"minecraft:raw_copper", 0, 0.7f},
        {"minecraft:raw_gold", 0, 1.0f},
        {"minecraft:raw_iron", 0, 0.7f},
        {"minecraft:red_sandstone", 0, 0.1f},
        {"minecraft:redstone_ore", 0, 0.7f},
        {"minecraft:salmon", 0, 0.35f},
        {"minecraft:sand", 0, 0.1f},
        {"minecraft:sandstone", 0, 0.1f},
        {"minecraft:sea_pickle", 0, 0.2f},
        {"minecraft:stained_hardened_clay", 0, 0.1f},
        {"minecraft:stained_hardened_clay", 1, 0.1f},
        {"minecraft:stained_hardened_clay", 2, 0.1f},
        {"minecraft:stained_hardened_clay", 3, 0.1f},
        {"minecraft:stained_hardened_clay", 4, 0.1f},
        {"minecraft:stained_hardened_clay", 5, 0.1f},
        {"minecraft:stained_hardened_clay", 6, 0.1f},
        {"minecraft:stained_hardened_clay", 7, 0.1f},
        {"minecraft:stained_hardened_clay", 8, 0.1f},
        {"minecraft:stained_hardened_clay", 9, 0.1f},
        {"minecraft:stained_hardened_clay", 10, 0.1f},
        {"minecraft:stained_hardened_clay", 11, 0.1f},
        {"minecraft:stained_hardened_clay", 12, 0.1f},
        {"minecraft:stained_hardened_clay", 13, 0.1f},
        {"minecraft:stained_hardened_clay", 14, 0.1f},
        {"minecraft:stained_hardened_clay", 15, 0.1f},
        {"minecraft:stone", 0, 0.1f},
        {"minecraft:stonebrick", 0, 0.1f},
        {"minecraft:stripped_acacia_log", 0, 0.15f},
        {"minecraft:stripped_birch_log", 0, 0.15f},
        {"minecraft:stripped_dark_oak_log", 0, 0.15f},
        {"minecraft:stripped_jungle_log", 0, 0.15f},
        {"minecraft:stripped_mangrove_log", 0, 0.15f},
        {"minecraft:stripped_mangrove_wood", 0, 0.15f},
        {"minecraft:stripped_oak_log", 0, 0.15f},
        {"minecraft:stripped_spruce_log", 0, 0.15f},
        {"minecraft:wood", 0, 0.15f},
        {"minecraft:wood", 1, 0.15f},
        {"minecraft:wood", 2, 0.15f},
        {"minecraft:wood", 3, 0.15f},
        {"minecraft:wood", 4, 0.15f},
        {"minecraft:wood", 5, 0.15f},
        {"minecraft:wood", 8, 0.15f},
        {"minecraft:wood", 9, 0.15f},
        {"minecraft:wood", 10, 0.15f},
        {"minecraft:wood", 11, 0.15f},
        {"minecraft:wood", 12, 0.15f},
        {"minecraft:wood", 13, 0.15f}
    };

    const std::unordered_map<std::string, float> &experienceByIdentifier() {
        static const std::unordered_map<std::string, float> table = [] {
            std::unordered_map<std::string, float> resolved;
            const LegacyItemMapper &mapper = LegacyItemMapper::getInstance();
            for (const LegacyExperience &entry: LEGACY_EXPERIENCE)
                resolved[mapper.resolve(entry.mIdentifier, entry.mData)] = entry.mExperience;
            return resolved;
        }();
        return table;
    }
}

float FurnaceExperience::get(const std::string &inputIdentifier) {
    const std::unordered_map<std::string, float> &table = experienceByIdentifier();
    const auto found = table.find(inputIdentifier);
    return found == table.end() ? 0.0f : found->second;
}
