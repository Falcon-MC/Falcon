#include "Item/Items/BundleItem.h"

#include "Actor/ServerPlayer.h"
#include "Inventory/BundleInventory.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemStack.h"

#include <string>
#include <utility>
#include <vector>

namespace {
    const char *BUNDLE_IDENTIFIERS[] = {
            "minecraft:bundle",
            "minecraft:white_bundle",
            "minecraft:light_gray_bundle",
            "minecraft:gray_bundle",
            "minecraft:black_bundle",
            "minecraft:brown_bundle",
            "minecraft:red_bundle",
            "minecraft:orange_bundle",
            "minecraft:yellow_bundle",
            "minecraft:lime_bundle",
            "minecraft:green_bundle",
            "minecraft:cyan_bundle",
            "minecraft:light_blue_bundle",
            "minecraft:blue_bundle",
            "minecraft:purple_bundle",
            "minecraft:magenta_bundle",
            "minecraft:pink_bundle"
    };
}

BundleItem::BundleItem(const Item &base) : Item(base) {
}

bool BundleItem::matches(const std::string &identifier) {
    for (const char *candidate: BUNDLE_IDENTIFIERS) {
        if (identifier == candidate) {
            return true;
        }
    }
    return false;
}

bool BundleItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) item;

    PlayerInventory &inventory = player.getInventory();
    const int selectedSlot = inventory.getSelectedSlot();
    ItemStack held = inventory.getItemInHand();
    if (!BundleInventory::isBundle(held)) {
        return false;
    }

    BundleInventory::ensureBundleId(held);

    BundleInventory bundle(held, owner.getCodecContext());
    const int slot = bundle.getFirstOccupiedSlot();
    if (slot < 0) {
        return false;
    }

    ItemStack dropped = bundle.getContainerItem(slot);
    dropped.mUsingNetId = false;
    dropped.mNetId = 0;
    bundle.setContainerItem(slot, ItemStack::air());

    inventory.assignNetId(held);
    const int32_t bundleId = BundleInventory::getBundleId(held);
    const std::vector<ItemStack> contents = bundle.getContents();
    inventory.setItem(selectedSlot, std::move(held));

    owner._throwItem(player, dropped);
    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::BUNDLE_DROP_CONTENTS, player.getPosition(),
                         "minecraft:player");

    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, selectedSlot);
    player.getInventoryManager().syncBundle(inventory.getItem(selectedSlot), bundleId, contents);
    return true;
}
