#include "World/Item/SpyglassItem.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(SpyglassItem, 100);

SpyglassItem::SpyglassItem(const Item &base) : Item(base) {}

bool SpyglassItem::matches(const std::string &identifier) {
    return identifier == "minecraft:spyglass";
}

// the game holds the spyglass as an item use: the client zooms only while the flag stays on
bool SpyglassItem::onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) item;
    owner.playLevelSound(owner.getLevelFor(player), "item.spyglass.use", player.getPosition(), "minecraft:player");
    return true;
}

bool SpyglassItem::onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                               int32_t elapsedTicks) const {
    (void) item;
    (void) elapsedTicks;
    owner.playLevelSound(owner.getLevelFor(player), "item.spyglass.stop_using", player.getPosition(),
                         "minecraft:player");
    return true;
}
