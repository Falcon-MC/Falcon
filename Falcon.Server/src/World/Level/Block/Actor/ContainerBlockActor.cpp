#include "World/Level/Block/Actor/ContainerBlockActor.h"

#include "Protocol/Types/ItemDefinition.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Level.h"

#include <algorithm>

namespace {
    const char *TAG_PAGE = "page";
    const char *TAG_TOTAL_PAGES = "totalPages";
    const char *TAG_HAS_BOOK = "hasBook";
    const char *TAG_BOOK = "book";
    const char *TAG_FACING = "facing";
    const char *TAG_DISABLED_SLOTS = "disabled_slots";
    const char *TAG_CRAFTING_TICKS = "crafting_ticks_remaining";
    const char *STATE_CRAFTING = "crafting";
    const uint16_t ALL_SLOTS = 0x1FF;
    const char *TAG_ITEMS = "Items";
    const char *TAG_LAST_INTERACTED_SLOT = "LastInteractedSlot";
    const char *TAG_BOOKSHELF_BOOKS = "minecraft:bookshelf_books";
}

Tag ShulkerBoxBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    data.putByte(TAG_FACING, mFacing);
    return data;
}

void ShulkerBoxBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    mFacing = data.getByte(TAG_FACING, mFacing);
}

Tag ShulkerBoxBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putByte(TAG_FACING, mFacing);
    return data;
}

Tag ShelfBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.put("Items", mInventory.saveItems());
    return data;
}

Tag ChiseledBookshelfBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    std::vector<Tag> items;
    for (int slot = 0; slot < SIZE; ++slot)
        items.push_back(ItemStackNbt::write(mInventory.getContainerItem(slot)));
    data.put(TAG_ITEMS, Tag::ofList(Tag::Type::Compound, std::move(items)));
    data.putInt(TAG_LAST_INTERACTED_SLOT, mLastInteractedSlot);
    return data;
}

// the game reads the slot from the position in Items and drops anything that is not a book
void ChiseledBookshelfBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mInventory.clear();
    const Tag *items = data.get(TAG_ITEMS);
    if (items != nullptr && items->isList()) {
        int slot = 0;
        for (const Tag &entry: items->getList()) {
            if (slot >= SIZE)
                break;
            ItemStack book = ItemStackNbt::read(entry, context);
            if (isBook(book)) {
                book.mCount = 1;
                mInventory.setContainerItem(slot, std::move(book));
            }
            ++slot;
        }
    }
    mLastInteractedSlot = std::clamp(data.getInt(TAG_LAST_INTERACTED_SLOT, 0), 0, SIZE);
}

Tag ChiseledBookshelfBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    const Tag saved = saveNbt();
    data.put(TAG_ITEMS, *saved.get(TAG_ITEMS));
    data.putInt(TAG_LAST_INTERACTED_SLOT, mLastInteractedSlot);
    return data;
}

bool ChiseledBookshelfBlockActor::isBook(const ItemStack &item) {
    if (item.isAir() || item.mCount <= 0 || item.mDefinition == nullptr)
        return false;
    const std::vector<std::string> &tags = CraftingRecipeTable::getItemTags(std::string(item.mDefinition->getIdentifier()));
    return std::find(tags.begin(), tags.end(), TAG_BOOKSHELF_BOOKS) != tags.end();
}

bool ChiseledBookshelfBlockActor::insertBook(int slot, const ItemStack &book) {
    if (slot < 0 || slot >= SIZE || !isBook(book) || !mInventory.getContainerItem(slot).isAir())
        return false;
    ItemStack single = book;
    single.mCount = 1;
    mInventory.setContainerItem(slot, std::move(single));
    mLastInteractedSlot = slot + 1;
    return true;
}

ItemStack ChiseledBookshelfBlockActor::takeBook(int slot) {
    if (slot < 0 || slot >= SIZE)
        return ItemStack::air();
    ItemStack book = mInventory.getContainerItem(slot);
    if (book.isAir())
        return book;
    mInventory.setContainerItem(slot, ItemStack::air());
    mLastInteractedSlot = slot + 1;
    return book;
}

int ChiseledBookshelfBlockActor::getBooksStored() const {
    int bits = 0;
    for (int slot = 0; slot < SIZE; ++slot) {
        if (!mInventory.getContainerItem(slot).isAir())
            bits |= 1 << slot;
    }
    return bits;
}

Tag ContainerBlockActor::saveNbt() const {
    return mInventory.saveNbt();
}

void ContainerBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mInventory.loadNbt(data, context);
}

// nothing at all without a book, else the book and where it is open
Tag LecternBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    if (!hasBook())
        return data;
    data.putBool(TAG_HAS_BOOK, true);
    data.putInt(TAG_PAGE, mPage);
    data.putInt(TAG_TOTAL_PAGES, mTotalPages);
    data.put(TAG_BOOK, ItemStackNbt::write(getBook()));
    return data;
}

void LecternBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mInventory.setContainerItem(0, ItemStack::air());
    mPage = 0;
    mTotalPages = 0;
    const Tag *book = data.get(TAG_BOOK);
    if (!data.getBool(TAG_HAS_BOOK, false) || book == nullptr)
        return;
    mInventory.setContainerItem(0, ItemStackNbt::read(*book, context));
    mPage = data.getInt(TAG_PAGE);
    mTotalPages = data.getInt(TAG_TOTAL_PAGES);
}

void LecternBlockActor::setBook(ItemStack book) {
    book.mCount = 1;
    const Tag *pages = book.mTag.isCompound() ? book.mTag.get("pages") : nullptr;
    mTotalPages = pages != nullptr && pages->isList() ? (int) pages->getList().size() : 0;
    mPage = 0;
    mInventory.setContainerItem(0, std::move(book));
}

ItemStack LecternBlockActor::takeBook() {
    ItemStack book = getBook();
    mInventory.setContainerItem(0, ItemStack::air());
    mPage = 0;
    mTotalPages = 0;
    return book;
}

Tag CrafterBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    data.putShort(TAG_DISABLED_SLOTS, (int16_t) mInventory.getDisabledSlots());
    return data;
}

void CrafterBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    mInventory.setDisabledSlots((uint16_t) data.getShort(TAG_DISABLED_SLOTS, 0) & ALL_SLOTS);
}

Tag CrafterBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putShort(TAG_DISABLED_SLOTS, (int16_t) mInventory.getDisabledSlots());
    data.putInt(TAG_CRAFTING_TICKS, mCraftingTicks);
    return data;
}

bool CrafterBlockActor::tick(ServerNetworkHandler &owner) {
    (void) owner;
    if (mCraftingTicks <= 0 || --mCraftingTicks > 0 || mLevel == nullptr)
        return true;

    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr || state->mStates.getByte(STATE_CRAFTING, 0) == 0)
        return true;

    Tag states = state->mStates;
    states.putByte(STATE_CRAFTING, 0);
    mLevel->setBlock(mPosition, BlockState(state->mName, states), false);
    return true;
}

bool CrafterBlockActor::setSlotDisabled(int slot, bool disabled) {
    if (slot < 0 || slot >= SIZE)
        return false;
    if (disabled && !mInventory.getContainerItem(slot).isAir())
        return false;

    const uint16_t before = mInventory.getDisabledSlots();
    const uint16_t bit = (uint16_t) (1u << slot);
    mInventory.setDisabledSlots(disabled ? (uint16_t) (before | bit) : (uint16_t) (before & ~bit));
    return mInventory.getDisabledSlots() != before;
}

int CrafterBlockActor::getComparatorSignal() const {
    int signal = 0;
    for (int slot = 0; slot < SIZE; ++slot) {
        const ItemStack &item = mInventory.getContainerItem(slot);
        if (mInventory.isSlotDisabled(slot) || (!item.isAir() && item.mCount > 0))
            ++signal;
    }
    return signal;
}

namespace {
    const char *TAG_SHERDS = "sherds";
}

Tag DecoratedPotBlockActor::saveNbt() const {
    Tag data = ContainerBlockActor::saveNbt();
    if (mSherds.isList())
        data.put(TAG_SHERDS, mSherds);
    return data;
}

Tag DecoratedPotBlockActor::getSpawnCompound() const {
    Tag data = ContainerBlockActor::getSpawnCompound();
    if (mSherds.isList())
        data.put(TAG_SHERDS, mSherds);
    return data;
}

void DecoratedPotBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    ContainerBlockActor::loadNbt(data, context);
    const Tag *sherds = data.get(TAG_SHERDS);
    mSherds = sherds != nullptr && sherds->isList() ? *sherds : Tag();
}
