#include "World/Inventory/BundleInventory.h"

#include "World/Inventory/ItemStackNbt.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/BundleItem.h"

#include <atomic>
#include <utility>

const char *BundleInventory::TAG_BUNDLE_ID = "bundle_id";
const char *BundleInventory::TAG_STORAGE_CONTENT = "storage_item_component_content";

namespace {
    std::atomic<int32_t> gNextBundleId(0);
}

bool BundleInventory::isBundle(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr) {
        return false;
    }
    return BundleItem::matches(item.mDefinition->getIdentifier());
}

int32_t BundleInventory::nextBundleId() {
    return gNextBundleId.fetch_add(1);
}

int32_t BundleInventory::getBundleId(const ItemStack &bundle) {
    if (bundle.mTag.getType() != Tag::Type::Compound) {
        return -1;
    }
    if (!bundle.mTag.contains(TAG_BUNDLE_ID, Tag::Type::Int)) {
        return -1;
    }

    const int32_t id = bundle.mTag.getInt(TAG_BUNDLE_ID);
    int32_t next = gNextBundleId.load();
    while (id >= next && !gNextBundleId.compare_exchange_weak(next, id + 1)) {
    }

    return id;
}

bool BundleInventory::ensureBundleId(ItemStack &bundle) {
    if (!isBundle(bundle)) {
        return false;
    }

    if (bundle.mTag.getType() != Tag::Type::Compound) {
        bundle.mTag = Tag::ofCompound();
    }

    bool changed = false;
    if (!bundle.mTag.contains(TAG_BUNDLE_ID, Tag::Type::Int)) {
        bundle.mTag.putInt(TAG_BUNDLE_ID, nextBundleId());
        changed = true;
    }

    if (!bundle.mTag.contains(TAG_STORAGE_CONTENT, Tag::Type::List)) {
        bundle.mTag.put(TAG_STORAGE_CONTENT, Tag::ofList(Tag::Type::Compound));
        changed = true;
    }

    return changed;
}

std::vector<ItemStack> BundleInventory::readContents(const ItemStack &bundle, const PacketCodecContext &context) {
    std::vector<ItemStack> contents((size_t) SIZE, ItemStack::air());
    if (bundle.mTag.getType() != Tag::Type::Compound) {
        return contents;
    }

    const Tag *list = bundle.mTag.get(TAG_STORAGE_CONTENT);
    if (list == nullptr || list->getType() != Tag::Type::List) {
        return contents;
    }

    for (const Tag &entry: list->getList()) {
        if (entry.getType() != Tag::Type::Compound) {
            continue;
        }

        const int slot = ItemStackNbt::readSlot(entry);
        if (slot < 0 || slot >= SIZE) {
            continue;
        }

        contents[(size_t) slot] = ItemStackNbt::read(entry, context);
    }

    return contents;
}

void BundleInventory::writeContents(ItemStack &bundle, const std::vector<ItemStack> &contents) {
    if (bundle.mTag.getType() != Tag::Type::Compound) {
        bundle.mTag = Tag::ofCompound();
    }

    Tag list = Tag::ofList(Tag::Type::Compound);
    for (size_t slot = 0; slot < contents.size(); ++slot) {
        const ItemStack &item = contents[slot];
        if (item.isAir() || item.mCount <= 0) {
            continue;
        }
        list.addToList(ItemStackNbt::write(item, (int) slot));
    }

    bundle.mTag.put(TAG_STORAGE_CONTENT, std::move(list));
}

int BundleInventory::getWeight(const ItemStack &item, const PacketCodecContext &context) {
    if (item.isAir() || item.mCount <= 0) {
        return 0;
    }

    if (isBundle(item)) {
        return getTotalWeight(readContents(item, context), context) + NESTED_BUNDLE_WEIGHT;
    }

    const int maxStackSize = PlayerInventory::getMaxStackSize(item);
    if (maxStackSize <= 0) {
        return MAX_FILL;
    }

    return (MAX_FILL / maxStackSize) * item.mCount;
}

int BundleInventory::getTotalWeight(const std::vector<ItemStack> &contents, const PacketCodecContext &context) {
    int weight = 0;
    for (const ItemStack &item: contents) {
        weight += getWeight(item, context);
    }
    return weight;
}

bool BundleInventory::prepareBundleIds(PlayerInventory &inventory, std::vector<int> &outTouchedSlots) {
    bool changed = false;

    for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
        ItemStack item = inventory.getItem(slot);
        if (!isBundle(item)) {
            continue;
        }
        if (ensureBundleId(item)) {
            inventory.setItem(slot, std::move(item));
            outTouchedSlots.push_back(slot);
            changed = true;
        }
    }

    ItemStack offhand = inventory.getOffhand();
    if (isBundle(offhand) && ensureBundleId(offhand)) {
        inventory.setOffhand(std::move(offhand));
        changed = true;
    }

    ItemStack cursor = inventory.getCursor();
    if (isBundle(cursor) && ensureBundleId(cursor)) {
        inventory.setCursor(std::move(cursor));
        changed = true;
    }

    return changed;
}

bool BundleInventory::prepareBundleIds(Container &container) {
    bool changed = false;
    const int size = container.getContainerSize();

    for (int slot = 0; slot < size; ++slot) {
        ItemStack item = container.getContainerItem(slot);
        if (!isBundle(item)) {
            continue;
        }
        if (ensureBundleId(item)) {
            container.setContainerItem(slot, std::move(item));
            changed = true;
        }
    }

    return changed;
}

BundleInventory::BundleInventory(ItemStack &bundle, const PacketCodecContext &context)
        : mBundle(bundle), mContext(context), mContents(readContents(bundle, context)) {
}

int BundleInventory::getContainerSize() const {
    return SIZE;
}

const ItemStack &BundleInventory::getContainerItem(int slot) const {
    if (slot < 0 || slot >= SIZE) {
        return PlayerInventory::getEmptyItem();
    }
    return mContents[(size_t) slot];
}

void BundleInventory::setContainerItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= SIZE) {
        return;
    }

    mContents[(size_t) slot] = std::move(item);
    save();
}

int BundleInventory::getFirstOccupiedSlot() const {
    for (int slot = 0; slot < SIZE; ++slot) {
        if (!mContents[(size_t) slot].isAir() && mContents[(size_t) slot].mCount > 0) {
            return slot;
        }
    }
    return -1;
}

void BundleInventory::save() {
    writeContents(mBundle, mContents);
}
