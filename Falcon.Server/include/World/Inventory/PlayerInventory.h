#pragma once

#include "World/Inventory/Container.h"
#include "Protocol/Types/ContainerSlotType.h"

#include <cstdint>
#include <utility>
#include <vector>

class PlayerInventory : public Container {
public:
    static const int HOTBAR_SIZE = 9;
    static const int CONTAINER_SIZE = 36;
    static const int ARMOR_SIZE = 4;
    static const int CRAFTING_SIZE = 4;
    static const int CRAFTING_TABLE_SIZE = 9;
    static const int FURNACE_SIZE = 3;
    static const int SAVED_SIZE = CONTAINER_SIZE + ARMOR_SIZE;

    static const int CONTAINER_ID_INVENTORY = 0;
    static const int CONTAINER_ID_OFFHAND = 119;
    static const int CONTAINER_ID_ARMOR = 120;
    static const int CONTAINER_ID_UI = 124;

    static const int OFFHAND_NETWORK_SLOT = 1;
    static const int CRAFTING_NETWORK_SLOT_FIRST = 28;
    static const int CRAFTING_TABLE_NETWORK_SLOT_FIRST = 32;

    static const int ARMOR_HEAD = 0;
    static const int ARMOR_TORSO = 1;
    static const int ARMOR_LEGS = 2;
    static const int ARMOR_FEET = 3;

    PlayerInventory();

    static const ItemStack &getEmptyItem();

    const std::vector<ItemStack> &getContents() const { return mItems; }

    const std::vector<ItemStack> &getArmorContents() const { return mArmor; }

    const std::vector<ItemStack> &getCraftingContents() const {
        return mCrafting;
    }

    const ItemStack &getItem(int slot) const;

    void setItem(int slot, ItemStack item);

    int getContainerSize() const override { return CONTAINER_SIZE; }

    const ItemStack &getContainerItem(int slot) const override { return getItem(slot); }

    void setContainerItem(int slot, ItemStack item) override { setItem(slot, std::move(item)); }

    const ItemStack &getArmor(int slot) const;

    void setArmor(int slot, ItemStack item);

    const ItemStack &getCraftingItem(int slot) const;

    void setCraftingItem(int slot, ItemStack item);

    const std::vector<ItemStack> &getCraftingTableContents() const { return mCraftingTable; }

    const ItemStack &getCraftingTableItem(int slot) const;

    void setCraftingTableItem(int slot, ItemStack item);

    const std::vector<ItemStack> &getFurnaceContents() const { return mFurnace; }

    const ItemStack &getFurnaceItem(int slot) const;

    void setFurnaceItem(int slot, ItemStack item);

    const ItemStack &getOffhand() const { return mOffhand; }

    void setOffhand(ItemStack item);

    const ItemStack &getCursor() const { return mCursor; }

    void setCursor(ItemStack item);

    int getSelectedSlot() const { return mSelectedSlot; }

    void setSelectedSlot(int slot);

    const ItemStack &getItemInHand() const { return getItem(mSelectedSlot); }

    void setItemInHand(ItemStack item) { setItem(mSelectedSlot, std::move(item)); }

    ItemStack *resolveSlot(ContainerSlotType container, int slot);

    const ItemStack *resolveSlot(ContainerSlotType container, int slot) const;

    int getFirstEmptySlot() const;

    bool addItem(const ItemStack &item);

    bool addItem(const ItemStack &item, std::vector<int> &outTouchedSlots);

    int addItemPartial(const ItemStack &item, std::vector<int> &outTouchedSlots);

    void clear();

    void assignNetId(ItemStack &item);

    static bool canStack(const ItemStack &left, const ItemStack &right);

    static int getMaxStackSize(const ItemStack &item);

private:
    std::vector<ItemStack> mItems;
    std::vector<ItemStack> mArmor;
    std::vector<ItemStack> mCrafting;
    std::vector<ItemStack> mCraftingTable;
    std::vector<ItemStack> mFurnace;
    ItemStack mOffhand;
    ItemStack mCursor;
    ItemStack mEnchantingInput;
    ItemStack mEnchantingMaterial;
    int mSelectedSlot;
    int32_t mNextNetId;
};
