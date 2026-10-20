#pragma once

#include "Core/NBT/Tag.h"
#include "World/Inventory/Container.h"

#include <array>

class PacketCodecContext;

class ChestInventory : public Container {
public:
    static const int SIZE = 27;
    static const int DOUBLE_SIZE = SIZE * 2;

    int getContainerSize() const override { return SIZE; }

    const ItemStack &getContainerItem(int slot) const override;

    void setContainerItem(int slot, ItemStack item) override;

    bool isEmpty() const;

    void clear();

    Tag saveNbt() const;

    void loadNbt(const Tag &data, const PacketCodecContext &context);

private:
    std::array<ItemStack, SIZE> mItems;
};

class DoubleChestInventory : public Container {
public:
    DoubleChestInventory(ChestInventory &left, ChestInventory &right) : mLeft(left), mRight(right) {}

    int getContainerSize() const override { return ChestInventory::DOUBLE_SIZE; }

    const ItemStack &getContainerItem(int slot) const override;

    void setContainerItem(int slot, ItemStack item) override;

private:
    ChestInventory &mLeft;
    ChestInventory &mRight;
};
