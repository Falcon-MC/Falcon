#include "World/Item/EmptyMapItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(EmptyMapItem, 100);

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/MapItem.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/MapHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>

namespace {
    // LevelSoundEvent 275, which sub_147B9F900 plays at the player
    const char *const USE_SOUND = "ui.cartography_table.take_result";
}

EmptyMapItem::EmptyMapItem(const Item &base) : Item(base) {}

bool EmptyMapItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool EmptyMapItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    ItemStack map = owner.createItemStack(MapItem::IDENTIFIER, 1);
    if (map.isAir())
        return false;

    map.mTag = Tag::ofCompound();
    if (item.mDamage == (int32_t) MapItemType::Locator)
        map.mTag.putByte(MapItem::TAG_DISPLAY_PLAYERS, 1);
    if (item.mTag.isCompound() && item.mTag.contains("display", Tag::Type::Compound))
        map.mTag.put("display", *item.mTag.get("display"));
    map.mTag.putInt(MapItem::TAG_NAME_INDEX, player.takeMapNameIndex());

    const int32_t scale = item.mTag.isCompound() ? item.mTag.getInt(MapItem::TAG_SCALE, 0) : 0;
    const Vector3f &position = player.getPosition();
    Level &mapLevel = MapItem::getMapLevel(owner, player);
    MapItemSavedData &data = mapLevel.getMaps().createMapSavedData(
            [&owner]() { return owner.allocateActorUniqueId(); },
            Vector3f(std::floor(position.x), std::floor(position.y), std::floor(position.z)),
            Dimension::toId(player.getDimension()), (int8_t) scale);
    MapItem::setMapId(map, data);
    MapHandler::broadcastCreation(owner, owner.getWorldFor(player), data);

    PlayerInventory &inventory = player.getInventory();
    const bool creative = player.getGameType() == (int32_t) GameType::Creative;
    ItemStack remaining = item;
    if (!creative)
        remaining.mCount -= 1;

    if (creative || remaining.mCount > 0) {
        if (!creative) {
            inventory.setItemInHand(remaining);
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                                  inventory.getSelectedSlot());
        }

        std::vector<int> touched;
        if (!inventory.addItem(map, touched))
            owner._throwItem(player, map);
        for (const int slot: touched)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
    } else {
        inventory.setItemInHand(map);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }

    InventoryHandler::sendHeldItem(owner, player);
    owner.playLevelSound(owner.getLevelFor(player), USE_SOUND, position);
    return true;
}
