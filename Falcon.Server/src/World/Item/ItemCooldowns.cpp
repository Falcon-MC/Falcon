#include "World/Item/ItemCooldowns.h"

namespace {
    struct CooldownDefinition {
        const char *mIdentifier;
        const char *mCategory;
        int mDuration;
    };

    const CooldownDefinition DEFINITIONS[] = {{"minecraft:chorus_fruit", "minecraft:chorusfruit", 20},
                                              {"minecraft:copper_spear", "minecraft:spear", 17},
                                              {"minecraft:diamond_spear", "minecraft:spear", 21},
                                              {"minecraft:ender_pearl", "minecraft:ender_pearl", 20},
                                              {"minecraft:goat_horn", "minecraft:goat_horn", 140},
                                              {"minecraft:golden_spear", "minecraft:spear", 19},
                                              {"minecraft:ice_bomb", "minecraft:ice_bomb", 10},
                                              {"minecraft:iron_spear", "minecraft:spear", 19},
                                              {"minecraft:netherite_spear", "minecraft:spear", 23},
                                              {"minecraft:shield", "minecraft:shield", 100},
                                              {"minecraft:stone_spear", "minecraft:spear", 15},
                                              {"minecraft:wind_charge", "minecraft:wind_charge", 10},
                                              {"minecraft:wooden_spear", "minecraft:spear", 13}};

    const CooldownDefinition *find(const std::string &identifier) {
        for (const CooldownDefinition &definition: DEFINITIONS) {
            if (identifier == definition.mIdentifier)
                return &definition;
        }
        return nullptr;
    }

    const CooldownDefinition *find(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return nullptr;
        return find(item.mDefinition->getIdentifier());
    }
}

int ItemCooldowns::getDuration(const ItemStack &item) {
    const CooldownDefinition *definition = find(item);
    return definition == nullptr ? 0 : definition->mDuration;
}

int ItemCooldowns::getDuration(const std::string &identifier) {
    const CooldownDefinition *definition = find(identifier);
    return definition == nullptr ? 0 : definition->mDuration;
}

std::string ItemCooldowns::getCategory(const ItemStack &item) {
    const CooldownDefinition *definition = find(item);
    return definition == nullptr ? std::string() : definition->mCategory;
}

std::string ItemCooldowns::getCategory(const std::string &identifier) {
    const CooldownDefinition *definition = find(identifier);
    return definition == nullptr ? std::string() : definition->mCategory;
}

bool ItemCooldowns::isSupported(const ItemStack &item) {
    return find(item) != nullptr;
}
