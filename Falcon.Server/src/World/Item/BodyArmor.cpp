#include "World/Item/BodyArmor.h"

namespace {
    // horse and nautilus armor read their points from a per tier table; the wolf armor is a fixed 11
    const BodyArmorData BODY_ARMOR[] = {
            {"minecraft:leather_horse_armor", 3, 0, 0.0f, false, nullptr},
            {"minecraft:copper_horse_armor", 4, 0, 0.0f, false, nullptr},
            {"minecraft:iron_horse_armor", 5, 0, 0.0f, false, nullptr},
            {"minecraft:golden_horse_armor", 7, 0, 0.0f, false, nullptr},
            {"minecraft:diamond_horse_armor", 11, 2, 0.0f, false, nullptr},
            {"minecraft:netherite_horse_armor", 19, 3, 0.1f, false, nullptr},
            {"minecraft:copper_nautilus_armor", 4, 0, 0.0f, false, nullptr},
            {"minecraft:iron_nautilus_armor", 5, 0, 0.0f, false, nullptr},
            {"minecraft:golden_nautilus_armor", 7, 0, 0.0f, false, nullptr},
            {"minecraft:diamond_nautilus_armor", 11, 2, 0.0f, false, nullptr},
            {"minecraft:netherite_nautilus_armor", 19, 3, 0.1f, false, nullptr},
            {"minecraft:wolf_armor", 11, 0, 0.0f, true, "armor.break_wolf"},
    };
}

const BodyArmorData *BodyArmor::find(const std::string &identifier) {
    for (const BodyArmorData &entry: BODY_ARMOR) {
        if (identifier == entry.mIdentifier)
            return &entry;
    }
    return nullptr;
}

const BodyArmorData *BodyArmor::find(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr)
        return nullptr;
    return find(item.mDefinition->getIdentifier());
}
