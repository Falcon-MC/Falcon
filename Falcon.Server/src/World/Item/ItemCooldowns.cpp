#include "World/Item/ItemCooldowns.h"

namespace {
    struct CooldownDefinition {
        const char *mIdentifier;
        const char *mCategory;
        int mDuration;
    };

    const CooldownDefinition DEFINITIONS[] = {
        {"minecraft:chorus_fruit", "minecraft:chorus_fruit", 20},
        {"minecraft:ender_pearl", "minecraft:ender_pearl", 20},
        {"minecraft:goat_horn", "minecraft:goat_horn", 140},
        {"minecraft:ice_bomb", "minecraft:ice_bomb", 10},
        {"minecraft:wind_charge", "minecraft:wind_charge", 10}
    };

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
