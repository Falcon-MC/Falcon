#include "Block/Inventory/SimpleContainerInventory.h"

#include "Block/Blocks/ContainerBlock.h"
#include "Inventory/ItemStackNbt.h"
#include "Inventory/PlayerInventory.h"

#include <utility>

namespace {
    const char *TAG_ITEMS = "Items";

    const ItemStack &emptyItem() {
        static const ItemStack air = ItemStack::air();
        return air;
    }
}

SimpleContainerInventory::SimpleContainerInventory(int size) {
    mItems.resize(size < 0 ? 0 : (size_t) size);
}

const ItemStack &SimpleContainerInventory::getContainerItem(int slot) const {
    if (slot < 0 || (size_t) slot >= mItems.size())
        return emptyItem();

    return mItems[(size_t) slot];
}

bool SimpleContainerInventory::canHold(const ItemStack &item) const {
    return !mRejectsShulkerBoxes || !ContainerBlock::isShulkerBoxItem(item);
}

void SimpleContainerInventory::setContainerItem(int slot, ItemStack item) {
    if (slot < 0 || (size_t) slot >= mItems.size())
        return;

    mItems[(size_t) slot] = std::move(item);
}

bool SimpleContainerInventory::isEmpty() const {
    for (const ItemStack &item: mItems) {
        if (!item.isAir() && item.mCount > 0)
            return false;
    }

    return true;
}

bool SimpleContainerInventory::isFull() const {
    for (const ItemStack &item: mItems) {
        if (item.isAir() || item.mCount <= 0)
            return false;

        if (item.mCount < PlayerInventory::getMaxStackSize(item))
            return false;
    }

    return true;
}

void SimpleContainerInventory::clear() {
    for (ItemStack &item: mItems)
        item = ItemStack::air();
}

Tag SimpleContainerInventory::saveItems() const {
    std::vector<Tag> items;
    for (size_t slot = 0; slot < mItems.size(); ++slot) {
        const ItemStack &item = mItems[slot];
        if (!item.isAir() && item.mCount > 0)
            items.push_back(ItemStackNbt::write(item, (int) slot));
    }

    return Tag::ofList(Tag::Type::Compound, std::move(items));
}

void SimpleContainerInventory::loadItems(const Tag &items, const PacketCodecContext &context) {
    clear();

    if (items.getType() != Tag::Type::List)
        return;

    for (const Tag &entry: items.getList()) {
        const int slot = ItemStackNbt::readSlot(entry);
        if (slot < 0 || (size_t) slot >= mItems.size())
            continue;

        mItems[(size_t) slot] = ItemStackNbt::read(entry, context);
    }
}

Tag SimpleContainerInventory::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.put(TAG_ITEMS, saveItems());
    return data;
}

void SimpleContainerInventory::loadNbt(const Tag &data, const PacketCodecContext &context) {
    const Tag *items = data.get(TAG_ITEMS);
    if (items == nullptr) {
        clear();
        return;
    }

    loadItems(*items, context);
}
