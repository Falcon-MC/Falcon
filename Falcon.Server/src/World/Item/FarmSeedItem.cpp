#include "World/Item/FarmSeedItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(FarmSeedItem, 100);

namespace {
    // carrots and potatoes plant too, but are eaten
    const char *const SEEDS[] = {"minecraft:wheat_seeds",       "minecraft:beetroot_seeds", "minecraft:pumpkin_seeds",
                                 "minecraft:melon_seeds",       "minecraft:torchflower_seeds",
                                 "minecraft:pitcher_pod",       "minecraft:nether_wart"};
}

FarmSeedItem::FarmSeedItem(const Item &base) : Item(base) {}

bool FarmSeedItem::matches(const std::string &identifier) {
    for (const char *seed: SEEDS) {
        if (identifier == seed)
            return true;
    }
    return false;
}
