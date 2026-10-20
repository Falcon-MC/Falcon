#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Inventory/Container.h"

#include <utility>
#include <vector>

class PlayerInventory;

class CraftingTableInventory : public Container {
public:
    static const int SIZE = 9;
    static const int NETWORK_SLOT_FIRST = 32;
    static const int CREATED_OUTPUT_NETWORK_SLOT = 50;

    CraftingTableInventory(PlayerInventory &owner, const Vector3i &holder) : mOwner(owner), mHolder(holder) {}

    const ItemStack &getItem(int slot) const;

    void setItem(int slot, ItemStack item);

    const std::vector<ItemStack> &getContents() const;

    const Vector3i &getHolder() const { return mHolder; }

    int getContainerSize() const override { return SIZE; }

    const ItemStack &getContainerItem(int slot) const override { return getItem(slot); }

    void setContainerItem(int slot, ItemStack item) override { setItem(slot, std::move(item)); }

private:
    PlayerInventory &mOwner;
    Vector3i mHolder;
};
