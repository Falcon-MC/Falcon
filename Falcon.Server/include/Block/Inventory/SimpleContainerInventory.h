#pragma once

#include "Core/NBT/Tag.h"
#include "Inventory/Container.h"

#include <vector>

class PacketCodecContext;

class SimpleContainerInventory : public Container {
public:
    explicit SimpleContainerInventory(int size);

    int getContainerSize() const override { return (int) mItems.size(); }

    const ItemStack &getContainerItem(int slot) const override;

    void setContainerItem(int slot, ItemStack item) override;

    bool canHold(const ItemStack &item) const override;

    void setRejectsShulkerBoxes(bool rejects) {
        mRejectsShulkerBoxes = rejects;
    }

    bool isEmpty() const;

    bool isFull() const;

    void clear();

    Tag saveItems() const;

    void loadItems(const Tag &items, const PacketCodecContext &context);

    Tag saveNbt() const;

    void loadNbt(const Tag &data, const PacketCodecContext &context);

private:
    std::vector<ItemStack> mItems;
    bool mRejectsShulkerBoxes = false;
};
