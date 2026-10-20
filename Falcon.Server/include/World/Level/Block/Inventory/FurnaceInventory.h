#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "World/Inventory/Container.h"

#include <array>
#include <cstdint>

class PacketCodecContext;

enum class FurnaceKind : uint8_t {
    Furnace,
    BlastFurnace,
    Smoker
};

class FurnaceInventory : public Container {
public:
    static const int SLOT_INPUT = 0;
    static const int SLOT_FUEL = 1;
    static const int SLOT_OUTPUT = 2;
    static const int SIZE = 3;

    static int cookDuration(FurnaceKind kind);

    static bool matches(const Vector3i &position, const Vector3i &other) { return position == other; }

    int getContainerSize() const override { return SIZE; }

    const ItemStack &getContainerItem(int slot) const override;

    void setContainerItem(int slot, ItemStack item) override;

    void clear();

    Tag saveNbt() const;

    void loadNbt(const Tag &data, const PacketCodecContext &context);

    std::array<ItemStack, SIZE> mItems;
};
