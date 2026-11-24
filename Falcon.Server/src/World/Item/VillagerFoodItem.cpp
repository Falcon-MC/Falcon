#include "World/Item/VillagerFoodItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(VillagerFoodItem, 100);

namespace {
    const char *const BREAD = "minecraft:bread";
    const char *const CROPS[] = {"minecraft:carrot", "minecraft:potato", "minecraft:beetroot"};
    // one stack must hold it: the game never adds two slots up
    const int32_t BREAD_NEEDED = 3;
    const int32_t CROPS_NEEDED = 12;
}

VillagerFoodItem::VillagerFoodItem(const Item &base) : Item(base) {}

bool VillagerFoodItem::matches(const std::string &identifier) {
    if (identifier == BREAD)
        return true;
    for (const char *crop: CROPS) {
        if (identifier == crop)
            return true;
    }
    return false;
}

int32_t VillagerFoodItem::getVillagerBreedingCount() const {
    return getIdentifier() == BREAD ? BREAD_NEEDED : CROPS_NEEDED;
}
