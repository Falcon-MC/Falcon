#include "World/Level/Block/Actor/ContainerBlockActor.h"

#include "World/Inventory/ItemStackNbt.h"
#include "World/Level/Level.h"

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
