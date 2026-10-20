#include "World/Level/Block/Inventory/ChestInventory.h"

#include "World/Inventory/ItemStackNbt.h"

#include <utility>
#include <vector>

namespace {
    const char *TAG_ITEMS = "Items";

    const ItemStack &emptyItem() {
        static const ItemStack air = ItemStack::air();
        return air;
    }
}

const ItemStack &ChestInventory::getContainerItem(int slot) const {
    if (slot < 0 || slot >= SIZE)
        return emptyItem();

    return mItems[(size_t) slot];
}

void ChestInventory::setContainerItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= SIZE)
        return;

    mItems[(size_t) slot] = std::move(item);
}

bool ChestInventory::isEmpty() const {
    for (const ItemStack &item: mItems) {
        if (!item.isAir())
            return false;
    }
    return true;
}

void ChestInventory::clear() {
    for (ItemStack &item: mItems)
        item = ItemStack::air();
}

Tag ChestInventory::saveNbt() const {
    std::vector<Tag> items;
    for (int slot = 0; slot < SIZE; ++slot) {
        const ItemStack &item = mItems[(size_t) slot];
        if (!item.isAir())
            items.push_back(ItemStackNbt::write(item, slot));
    }

    Tag data = Tag::ofCompound();
    data.put(TAG_ITEMS, Tag::ofList(Tag::Type::Compound, std::move(items)));
    return data;
}

void ChestInventory::loadNbt(const Tag &data, const PacketCodecContext &context) {
    clear();

    const Tag *items = data.get(TAG_ITEMS);
    if (items == nullptr || items->getType() != Tag::Type::List)
        return;

    for (const Tag &entry: items->getList()) {
        const int slot = ItemStackNbt::readSlot(entry);
        if (slot < 0 || slot >= SIZE)
            continue;

        mItems[(size_t) slot] = ItemStackNbt::read(entry, context);
    }
}

const ItemStack &DoubleChestInventory::getContainerItem(int slot) const {
    if (slot < ChestInventory::SIZE)
        return mLeft.getContainerItem(slot);

    return mRight.getContainerItem(slot - ChestInventory::SIZE);
}

void DoubleChestInventory::setContainerItem(int slot, ItemStack item) {
    if (slot < ChestInventory::SIZE) {
        mLeft.setContainerItem(slot, std::move(item));
        return;
    }

    mRight.setContainerItem(slot - ChestInventory::SIZE, std::move(item));
}
