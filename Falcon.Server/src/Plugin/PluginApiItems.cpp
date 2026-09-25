#include "Actor/ServerPlayer.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginServerApi.h"
#include "Protocol/Types/ItemStack.h"

#include <string>
#include <utility>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    const char *const DISPLAY_TAG = "display";
    const char *const NAME_TAG = "Name";
    const char *const LORE_TAG = "Lore";

    FalconItem *copyOf(const ItemStack &stack) {
        ItemStack *copy = new ItemStack(stack);
        copy->mUsingNetId = false;
        copy->mNetId = 0;
        return toHandle(copy);
    }

    ItemStack valueOf(FalconItem *handle) {
        if (handle == nullptr)
            return ItemStack::air();
        ItemStack copy = *item(handle);
        copy.mUsingNetId = false;
        copy.mNetId = 0;
        if (copy.isAir() || copy.mCount <= 0)
            return ItemStack::air();
        return copy;
    }

    const Tag *displayOf(const ItemStack &stack) {
        if (!stack.mTag.isCompound())
            return nullptr;
        const Tag *display = stack.mTag.get(DISPLAY_TAG);
        if (display == nullptr || !display->isCompound())
            return nullptr;
        return display;
    }

    Tag &mutableDisplayOf(ItemStack &stack) {
        if (!stack.mTag.isCompound())
            stack.mTag = Tag::ofCompound();
        Tag *display = stack.mTag.get(DISPLAY_TAG);
        if (display == nullptr || !display->isCompound()) {
            stack.mTag.put(DISPLAY_TAG, Tag::ofCompound());
            display = stack.mTag.get(DISPLAY_TAG);
        }
        return *display;
    }

    void removeDisplayKey(ItemStack &stack, const std::string &key) {
        if (displayOf(stack) == nullptr)
            return;

        Tag *display = stack.mTag.get(DISPLAY_TAG);
        display->remove(key);
        if (display->isEmpty())
            stack.mTag.remove(DISPLAY_TAG);
        if (stack.mTag.isEmpty())
            stack.mTag = Tag();
    }

    const Tag *loreOf(const ItemStack &stack) {
        const Tag *display = displayOf(stack);
        if (display == nullptr)
            return nullptr;
        const Tag *lore = display->get(LORE_TAG);
        if (lore == nullptr || !lore->isList() || lore->getListType() != Tag::Type::String)
            return nullptr;
        return lore;
    }

    FalconItem *itemCreate(const char *identifier, uint32_t count) {
        if (identifier == nullptr)
            return nullptr;

        const int32_t amount = count > 0x7fffffffu ? 0x7fffffff : (int32_t) count;
        ItemStack stack = owner().createItemStack(identifier, amount);
        if (stack.isAir())
            return nullptr;
        return toHandle(new ItemStack(std::move(stack)));
    }

    FalconItem *itemCopy(FalconItem *handle) {
        if (handle == nullptr)
            return nullptr;
        return copyOf(*item(handle));
    }

    void itemDestroy(FalconItem *handle) {
        delete item(handle);
    }

    int itemIsEmpty(FalconItem *handle) {
        if (handle == nullptr)
            return 1;
        const ItemStack *stack = item(handle);
        return stack->isAir() || stack->mCount <= 0 ? 1 : 0;
    }

    const char *itemIdentifier(FalconItem *handle) {
        if (handle == nullptr || item(handle)->isAir())
            return "minecraft:air";
        return hold(item(handle)->mDefinition->getIdentifier());
    }

    uint32_t itemCount(FalconItem *handle) {
        if (handle == nullptr || item(handle)->isAir() || item(handle)->mCount < 0)
            return 0;
        return (uint32_t) item(handle)->mCount;
    }

    void itemSetCount(FalconItem *handle, uint32_t count) {
        if (handle == nullptr || item(handle)->isAir())
            return;
        item(handle)->mCount = count > 0x7fffffffu ? 0x7fffffff : (int) count;
    }

    int32_t itemDamage(FalconItem *handle) {
        if (handle == nullptr)
            return 0;
        return item(handle)->mDamage;
    }

    void itemSetDamage(FalconItem *handle, int32_t damage) {
        if (handle == nullptr || item(handle)->isAir())
            return;
        item(handle)->mDamage = damage < 0 ? 0 : damage;
    }

    const char *itemCustomName(FalconItem *handle) {
        if (handle == nullptr)
            return nullptr;
        const Tag *display = displayOf(*item(handle));
        if (display == nullptr || !display->contains(NAME_TAG, Tag::Type::String))
            return nullptr;
        return hold(display->getString(NAME_TAG));
    }

    void itemSetCustomName(FalconItem *handle, const char *name) {
        if (handle == nullptr || item(handle)->isAir())
            return;

        ItemStack &stack = *item(handle);
        if (name == nullptr || name[0] == '\0') {
            removeDisplayKey(stack, NAME_TAG);
            return;
        }
        mutableDisplayOf(stack).putString(NAME_TAG, name);
    }

    uint32_t itemLoreCount(FalconItem *handle) {
        if (handle == nullptr)
            return 0;
        const Tag *lore = loreOf(*item(handle));
        return lore == nullptr ? 0 : (uint32_t) lore->getList().size();
    }

    const char *itemLore(FalconItem *handle, uint32_t index) {
        if (handle == nullptr)
            return nullptr;
        const Tag *lore = loreOf(*item(handle));
        if (lore == nullptr || index >= lore->getList().size())
            return nullptr;
        return hold(lore->getList()[index].asString());
    }

    void itemSetLore(FalconItem *handle, const char *const *lines, uint32_t count) {
        if (handle == nullptr || item(handle)->isAir())
            return;

        ItemStack &stack = *item(handle);
        if (lines == nullptr || count == 0) {
            removeDisplayKey(stack, LORE_TAG);
            return;
        }

        std::vector<Tag> values;
        values.reserve(count);
        for (uint32_t index = 0; index < count; index++)
            values.push_back(Tag::ofString(lines[index] == nullptr ? std::string() : std::string(lines[index])));
        mutableDisplayOf(stack).put(LORE_TAG, Tag::ofList(Tag::Type::String, std::move(values)));
    }

    int32_t itemEnchantmentLevel(FalconItem *handle, uint32_t enchantment) {
        if (handle == nullptr)
            return 0;
        return ItemEnchantments::getLevel(*item(handle), (int32_t) enchantment);
    }

    void itemSetEnchantmentLevel(FalconItem *handle, uint32_t enchantment, int32_t level) {
        if (handle == nullptr || item(handle)->isAir())
            return;

        ItemStack &stack = *item(handle);
        const int32_t id = (int32_t) enchantment;
        std::vector<EnchantmentInstance> enchantments = ItemEnchantments::read(stack);

        bool found = false;
        for (auto it = enchantments.begin(); it != enchantments.end();) {
            if (it->mId != id) {
                ++it;
                continue;
            }
            found = true;
            if (level <= 0) {
                it = enchantments.erase(it);
                continue;
            }
            it->mLevel = level;
            ++it;
        }

        if (!found && level > 0)
            enchantments.push_back(EnchantmentInstance{id, level});
        ItemEnchantments::write(stack, enchantments);
    }

    uint32_t playerInventorySize(FalconPlayer *target) {
        return (uint32_t) player(target)->getInventory().getContainerSize();
    }

    FalconItem *playerInventoryItem(FalconPlayer *target, uint32_t slot) {
        const PlayerInventory &inventory = player(target)->getInventory();
        if (slot >= (uint32_t) inventory.getContainerSize())
            return nullptr;
        return copyOf(inventory.getItem((int) slot));
    }

    void playerSetInventoryItem(FalconPlayer *target, uint32_t slot, FalconItem *handle) {
        ServerPlayer *source = player(target);
        PlayerInventory &inventory = source->getInventory();
        if (slot >= (uint32_t) inventory.getContainerSize())
            return;

        inventory.setItem((int) slot, valueOf(handle));
        source->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, (int) slot);
        if ((int) slot == inventory.getSelectedSlot())
            InventoryHandler::sendHeldItem(owner(), *source);
    }

    int playerGiveItem(FalconPlayer *target, FalconItem *handle) {
        const ItemStack stack = valueOf(handle);
        if (stack.isAir())
            return 0;

        ServerPlayer *source = player(target);
        std::vector<int> touchedSlots;
        const int remaining = source->getInventory().addItemPartial(stack, touchedSlots);
        for (const int slot: touchedSlots)
            source->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        return remaining <= 0 ? 1 : 0;
    }

    uint32_t playerSelectedSlot(FalconPlayer *target) {
        return (uint32_t) player(target)->getInventory().getSelectedSlot();
    }

    void playerSetSelectedSlot(FalconPlayer *target, uint32_t slot) {
        if (slot >= (uint32_t) PlayerInventory::HOTBAR_SIZE)
            return;

        ServerPlayer *source = player(target);
        source->getInventory().setSelectedSlot((int) slot);
        InventoryHandler::sendHeldItem(owner(), *source);
    }

    FalconItem *playerArmorItem(FalconPlayer *target, FalconArmorSlot slot) {
        if (slot >= (uint32_t) PlayerInventory::ARMOR_SIZE)
            return nullptr;
        return copyOf(player(target)->getInventory().getArmor((int) slot));
    }

    void playerSetArmorItem(FalconPlayer *target, FalconArmorSlot slot, FalconItem *handle) {
        if (slot >= (uint32_t) PlayerInventory::ARMOR_SIZE)
            return;

        ServerPlayer *source = player(target);
        source->getInventory().setArmor((int) slot, valueOf(handle));
        source->getInventoryManager().syncSlot(InventoryManager::InventoryId::Armor, (int) slot);
        InventoryHandler::sendArmorContent(owner(), *source);
    }

    FalconItem *playerOffhandItem(FalconPlayer *target) {
        return copyOf(player(target)->getInventory().getOffhand());
    }

    void playerSetOffhandItem(FalconPlayer *target, FalconItem *handle) {
        ServerPlayer *source = player(target);
        source->getInventory().setOffhand(valueOf(handle));
        source->getInventoryManager().syncSlot(InventoryManager::InventoryId::Offhand, 0);
        InventoryHandler::sendOffhandContent(owner(), *source);
    }

    void playerClearInventory(FalconPlayer *target) {
        ServerPlayer *source = player(target);
        source->getInventory().clear();
        source->getInventoryManager().syncAll();
        InventoryHandler::sendArmorContent(owner(), *source);
        InventoryHandler::sendOffhandContent(owner(), *source);
        InventoryHandler::sendHeldItem(owner(), *source);
    }
}

void PluginServerApi::fillItems(FalconServerApi &api) {
    api.itemCreate = &itemCreate;
    api.itemCopy = &itemCopy;
    api.itemDestroy = &itemDestroy;
    api.itemIsEmpty = &itemIsEmpty;
    api.itemIdentifier = &itemIdentifier;
    api.itemCount = &itemCount;
    api.itemSetCount = &itemSetCount;
    api.itemDamage = &itemDamage;
    api.itemSetDamage = &itemSetDamage;
    api.itemCustomName = &itemCustomName;
    api.itemSetCustomName = &itemSetCustomName;
    api.itemLoreCount = &itemLoreCount;
    api.itemLore = &itemLore;
    api.itemSetLore = &itemSetLore;
    api.itemEnchantmentLevel = &itemEnchantmentLevel;
    api.itemSetEnchantmentLevel = &itemSetEnchantmentLevel;
    api.playerInventorySize = &playerInventorySize;
    api.playerInventoryItem = &playerInventoryItem;
    api.playerSetInventoryItem = &playerSetInventoryItem;
    api.playerGiveItem = &playerGiveItem;
    api.playerSelectedSlot = &playerSelectedSlot;
    api.playerSetSelectedSlot = &playerSetSelectedSlot;
    api.playerArmorItem = &playerArmorItem;
    api.playerSetArmorItem = &playerSetArmorItem;
    api.playerOffhandItem = &playerOffhandItem;
    api.playerSetOffhandItem = &playerSetOffhandItem;
    api.playerClearInventory = &playerClearInventory;
}
