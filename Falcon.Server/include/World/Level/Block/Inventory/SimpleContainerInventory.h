#pragma once

#include "Core/NBT/Tag.h"
#include "World/Inventory/Container.h"

#include <cstdint>
#include <vector>

class PacketCodecContext;

class SimpleContainerInventory : public Container {
public:
    explicit SimpleContainerInventory(int size);

    int getContainerSize() const override {
        return (int) mItems.size();
    }

    const ItemStack &getContainerItem(int slot) const override;

    void setContainerItem(int slot, ItemStack item) override;

    bool canHold(const ItemStack &item) const override;

    void setRejectsShulkerBoxes(bool rejects) {
        mRejectsShulkerBoxes = rejects;
    }

    bool isSlotDisabled(int slot) const override {
        return slot >= 0 && slot < 16 && (mDisabledSlots & (1u << slot)) != 0;
    }

    uint16_t getDisabledSlots() const {
        return mDisabledSlots;
    }

    void setDisabledSlots(uint16_t slots) {
        mDisabledSlots = slots;
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
    uint16_t mDisabledSlots = 0;
};
