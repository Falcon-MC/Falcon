#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ServerPlayer.h"
#include "Item/EnchantmentData.h"
#include "Item/Item.h"
#include "Item/ItemData.h"
#include "Item/ItemEnchantments.h"
#include "Item/StringToItemParser.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/PlayerStartItemCooldownPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <random>
#include <utility>

void ServerNetworkHandler::startPlayerItemCooldown(ServerPlayer &player, const std::string &category,
                                                   int32_t durationTicks) {
    PlayerStartItemCooldownPacket packet;
    packet.mItemCategory = category;
    packet.mCooldownDuration = durationTicks;
    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);
}

void ServerNetworkHandler::setPlayerEquipment(ServerPlayer &player, const std::string &slot,
                                              const std::string &typeId, int32_t amount, int32_t damage,
                                              const Tag &dynamicProperties) {
    ItemStack stack;
    if (!typeId.empty()) {
        Item item;
        if (StringToItemParser::getInstance().parse(typeId, item)) {
            stack.mDefinition = mItemDefinitions.getDefinition(item.getIdentifier());
            stack.mBlockDefinition = mBlockDefinitions.getDefinition(item.getIdentifier());
            stack.mCount = amount < 1 ? 1 : amount;
            stack.mDamage = damage < 0 ? 0 : damage;

            if (!dynamicProperties.isEmpty()) {
                if (!stack.mTag.isCompound())
                    stack.mTag = Tag::ofCompound();
                stack.mTag.put("DynamicProperties", dynamicProperties);
            }
        }
    }

    PlayerInventory &inventory = player.getInventory();
    if (slot == "Head") {
        inventory.setArmor(PlayerInventory::ARMOR_HEAD, stack);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Armor, PlayerInventory::ARMOR_HEAD);
    } else if (slot == "Chest") {
        inventory.setArmor(PlayerInventory::ARMOR_TORSO, stack);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Armor, PlayerInventory::ARMOR_TORSO);
    } else if (slot == "Legs") {
        inventory.setArmor(PlayerInventory::ARMOR_LEGS, stack);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Armor, PlayerInventory::ARMOR_LEGS);
    } else if (slot == "Feet") {
        inventory.setArmor(PlayerInventory::ARMOR_FEET, stack);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Armor, PlayerInventory::ARMOR_FEET);
    } else if (slot == "Offhand") {
        inventory.setOffhand(stack);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Offhand, 0);
    } else {
        inventory.setItemInHand(stack);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }
}

void ServerNetworkHandler::damagePlayerHeldItem(ServerPlayer &player, int32_t amount) {
    if (amount <= 0 || player.getGameType() == (int32_t) GameType::Creative)
        return;

    PlayerInventory &inventory = player.getInventory();
    ItemStack held = inventory.getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return;

    const ItemData *itemData = ItemDataTable::find(held.mDefinition->getIdentifier());
    if (itemData == nullptr || itemData->mMaxDurability <= 0)
        return;

    static std::mt19937 durabilityRng(0x9E3779B9u);
    const int32_t unbreaking = ItemEnchantments::getLevel(held, EnchantmentIds::UNBREAKING);

    int32_t applied = 0;
    for (int32_t i = 0; i < amount; i++) {
        if (unbreaking <= 0 || (durabilityRng() % (uint32_t) (unbreaking + 1)) == 0)
            applied++;
    }

    if (applied == 0)
        return;

    held.mDamage += applied;
    if (held.mDamage >= itemData->mMaxDurability) {
        inventory.setItemInHand(ItemStack::air());
        playLevelSound(getLevelFor(player), LevelSoundEvent::BREAK, player.getPosition(), "minecraft:player");
    } else {
        inventory.setItemInHand(std::move(held));
    }

    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
}

void ServerNetworkHandler::setContainerSlot(ServerPlayer &player, int32_t slot, const std::string &typeId,
                                            int32_t amount, const Tag &dynamicProperties) {
    ItemStack stack;
    if (!typeId.empty()) {
        Item item;
        if (StringToItemParser::getInstance().parse(typeId, item)) {
            stack.mDefinition = mItemDefinitions.getDefinition(item.getIdentifier());
            stack.mBlockDefinition = mBlockDefinitions.getDefinition(item.getIdentifier());
            stack.mCount = amount < 1 ? 1 : amount;
            if (!dynamicProperties.isEmpty()) {
                if (!stack.mTag.isCompound())
                    stack.mTag = Tag::ofCompound();
                stack.mTag.put("DynamicProperties", dynamicProperties);
            }
        }
    }

    PlayerInventory &inventory = player.getInventory();
    if (slot < 0 || slot >= PlayerInventory::CONTAINER_SIZE)
        return;

    inventory.setItem(slot, stack);
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
}
