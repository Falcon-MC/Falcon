#include "World/Level/Block/Inventory/FurnaceInventory.h"

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

int FurnaceInventory::cookDuration(FurnaceKind kind) {
    return kind == FurnaceKind::Furnace ? 200 : 100;
}

const ItemStack &FurnaceInventory::getContainerItem(int slot) const {
    if (slot < 0 || slot >= SIZE)
        return emptyItem();

    return mItems[(size_t) slot];
}

void FurnaceInventory::setContainerItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= SIZE)
        return;

    mItems[(size_t) slot] = std::move(item);
}

void FurnaceInventory::clear() {
    for (ItemStack &item: mItems)
        item = ItemStack::air();
}

Tag FurnaceInventory::saveNbt() const {
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

void FurnaceInventory::loadNbt(const Tag &data, const PacketCodecContext &context) {
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
