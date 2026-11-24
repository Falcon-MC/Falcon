#include "World/Item/FarmSeedItem.h"

#include "World/Item/ItemClassRegistry.h"
#include "World/Item/ItemNetworkIdTable.h"
#include "Protocol/Types/ItemStack.h"

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

const Tag *FarmSeedItem::getSeedComponent(const ItemStack &item) {
    if (item.isAir() || item.mCount <= 0)
        return nullptr;
    const ItemNetworkIdEntry *entry = ItemNetworkIdTable::find(item.mDefinition->getIdentifier());
    const Tag *components = entry == nullptr ? nullptr : entry->mComponents.get("components");
    return components == nullptr ? nullptr : components->get("minecraft:seed");
}

bool FarmSeedItem::plantsOn(const Tag &seed, const std::string &block) {
    const Tag *plantAt = seed.get("plant_at");
    if (plantAt == nullptr || !plantAt->isList())
        return false;
    for (const Tag &name: plantAt->getList()) {
        if (name.asString() == block)
            return true;
    }
    return false;
}
