#pragma once

#include "World/Inventory/Container.h"
#include "Protocol/PacketCodecContext.h"

#include <cstdint>
#include <vector>

class PlayerInventory;

class BundleInventory : public Container {
public:
    static constexpr int SIZE = 64;
    static constexpr int MAX_FILL = 64;
    static constexpr int NESTED_BUNDLE_WEIGHT = 4;

    static const char *TAG_BUNDLE_ID;
    static const char *TAG_STORAGE_CONTENT;

    static bool isBundle(const ItemStack &item);

    static int32_t nextBundleId();

    static int32_t getBundleId(const ItemStack &bundle);

    static bool ensureBundleId(ItemStack &bundle);

    static std::vector<ItemStack> readContents(const ItemStack &bundle, const PacketCodecContext &context);

    static void writeContents(ItemStack &bundle, const std::vector<ItemStack> &contents);

    static int getWeight(const ItemStack &item, const PacketCodecContext &context);

    static int getTotalWeight(const std::vector<ItemStack> &contents, const PacketCodecContext &context);

    static bool prepareBundleIds(PlayerInventory &inventory, std::vector<int> &outTouchedSlots);

    static bool prepareBundleIds(Container &container);

    BundleInventory(ItemStack &bundle, const PacketCodecContext &context);

    int getContainerSize() const override;

    const ItemStack &getContainerItem(int slot) const override;

    void setContainerItem(int slot, ItemStack item) override;

    const std::vector<ItemStack> &getContents() const { return mContents; }

    int getFirstOccupiedSlot() const;

    void save();

private:
    ItemStack &mBundle;
    const PacketCodecContext &mContext;
    std::vector<ItemStack> mContents;
};
