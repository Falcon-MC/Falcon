#include "World/Inventory/PlayerInventory.h"

#include "World/Level/Block/BlockData.h"
#include "World/Item/ItemData.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <utility>

namespace {
    const int DEFAULT_MAX_STACK_SIZE = 64;
}

PlayerInventory::PlayerInventory()
        : mItems((size_t) CONTAINER_SIZE), mArmor((size_t) ARMOR_SIZE), mCrafting((size_t) CRAFTING_SIZE),
          mCraftingTable((size_t) CRAFTING_TABLE_SIZE), mFurnace((size_t) FURNACE_SIZE),
          mSmithing((size_t) SMITHING_SIZE), mAnvil((size_t) ANVIL_SIZE),
          mCartography((size_t) CARTOGRAPHY_SIZE), mTrade((size_t) TRADE_SIZE), mSelectedSlot(0), mNextNetId(1) {
    mOffhand = ItemStack::air();
    mBodyArmor = ItemStack::air();
    mCursor = ItemStack::air();
    for (ItemStack &item: mSmithing) {
        item = ItemStack::air();
    }
    mStonecutterInput = ItemStack::air();
    mBeaconPayment = ItemStack::air();

    for (ItemStack &item: mItems) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mArmor) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mCrafting) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mCraftingTable) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mFurnace) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mAnvil) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mCartography) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mTrade) {
        item = ItemStack::air();
    }
}

const ItemStack &PlayerInventory::getEmptyItem() {
    static const ItemStack empty = ItemStack::air();
    return empty;
}

const ItemStack &PlayerInventory::getItem(int slot) const {
    if (slot < 0 || slot >= CONTAINER_SIZE) {
        return getEmptyItem();
    }
    return mItems[(size_t) slot];
}

void PlayerInventory::setItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= CONTAINER_SIZE) {
        return;
    }

    assignNetId(item);
    mItems[(size_t) slot] = std::move(item);
}

const ItemStack &PlayerInventory::getArmor(int slot) const {
    if (slot < 0 || slot >= ARMOR_SIZE) {
        return getEmptyItem();
    }
    return mArmor[(size_t) slot];
}

void PlayerInventory::setArmor(int slot, ItemStack item) {
    if (slot < 0 || slot >= ARMOR_SIZE) {
        return;
    }

    assignNetId(item);
    mArmor[(size_t) slot] = std::move(item);
}

const ItemStack &PlayerInventory::getCraftingItem(int slot) const {
    if (slot < 0 || slot >= CRAFTING_SIZE) {
        return getEmptyItem();
    }
    return mCrafting[(size_t) slot];
}

void PlayerInventory::setCraftingItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= CRAFTING_SIZE) {
        return;
    }

    assignNetId(item);
    mCrafting[(size_t) slot] = std::move(item);
}

const ItemStack &PlayerInventory::getCraftingTableItem(int slot) const {
    if (slot < 0 || slot >= CRAFTING_TABLE_SIZE) {
        return getEmptyItem();
    }
    return mCraftingTable[(size_t) slot];
}

void PlayerInventory::setCraftingTableItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= CRAFTING_TABLE_SIZE) {
        return;
    }

    assignNetId(item);
    mCraftingTable[(size_t) slot] = std::move(item);
}

const ItemStack &PlayerInventory::getFurnaceItem(int slot) const {
    if (slot < 0 || slot >= FURNACE_SIZE) {
        return getEmptyItem();
    }
    return mFurnace[(size_t) slot];
}

void PlayerInventory::setFurnaceItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= FURNACE_SIZE) {
        return;
    }

    assignNetId(item);
    mFurnace[(size_t) slot] = std::move(item);
}

const ItemStack &PlayerInventory::getAnvilItem(int slot) const {
    if (slot < 0 || slot >= ANVIL_SIZE) {
        return getEmptyItem();
    }
    return mAnvil[(size_t) slot];
}

void PlayerInventory::setAnvilItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= ANVIL_SIZE) {
        return;
    }

    assignNetId(item);
    mAnvil[(size_t) slot] = std::move(item);
}

const ItemStack &PlayerInventory::getCartographyItem(int slot) const {
    if (slot < 0 || slot >= CARTOGRAPHY_SIZE) {
        return getEmptyItem();
    }
    return mCartography[(size_t) slot];
}

void PlayerInventory::setCartographyItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= CARTOGRAPHY_SIZE) {
        return;
    }

    assignNetId(item);
    mCartography[(size_t) slot] = std::move(item);
}

const ItemStack &PlayerInventory::getTradeItem(int slot) const {
    if (slot < 0 || slot >= TRADE_SIZE) {
        return getEmptyItem();
    }
    return mTrade[(size_t) slot];
}

void PlayerInventory::setTradeItem(int slot, ItemStack item) {
    if (slot < 0 || slot >= TRADE_SIZE) {
        return;
    }

    assignNetId(item);
    mTrade[(size_t) slot] = std::move(item);
}

int PlayerInventory::grindstoneIndex(ContainerSlotType container) {
    switch (container) {
        case ContainerSlotType::GrindstoneInput: return 0;
        case ContainerSlotType::GrindstoneAdditional: return 1;
        default: return -1;
    }
}

void PlayerInventory::setGrindstoneItem(int index, ItemStack item) {
    assignNetId(item);
    mGrindstone[(size_t) index] = std::move(item);
}

int PlayerInventory::smithingIndex(ContainerSlotType container) {
    switch (container) {
        case ContainerSlotType::SmithingTableInput: return 0;
        case ContainerSlotType::SmithingTableMaterial: return 1;
        case ContainerSlotType::SmithingTableTemplate: return 2;
        default: return -1;
    }
}

void PlayerInventory::setSmithingItem(int index, ItemStack item) {
    assignNetId(item);
    mSmithing[(size_t) index] = std::move(item);
}

void PlayerInventory::setStonecutterInput(ItemStack item) {
    assignNetId(item);
    mStonecutterInput = std::move(item);
}

void PlayerInventory::setBeaconPayment(ItemStack item) {
    assignNetId(item);
    mBeaconPayment = std::move(item);
}

void PlayerInventory::setOffhand(ItemStack item) {
    assignNetId(item);
    mOffhand = std::move(item);
}

void PlayerInventory::setBodyArmor(ItemStack item) {
    assignNetId(item);
    mBodyArmor = std::move(item);
}

void PlayerInventory::setCursor(ItemStack item) {
    assignNetId(item);
    mCursor = std::move(item);
}

void PlayerInventory::setSelectedSlot(int slot) {
    if (slot < 0) {
        mSelectedSlot = 0;
        return;
    }

    if (slot >= HOTBAR_SIZE) {
        mSelectedSlot = HOTBAR_SIZE - 1;
        return;
    }

    mSelectedSlot = slot;
}

ItemStack *PlayerInventory::resolveSlot(ContainerSlotType container, int slot) {
    const PlayerInventory *self = this;
    return const_cast<ItemStack *>(self->resolveSlot(container, slot));
}

const ItemStack *PlayerInventory::resolveSlot(ContainerSlotType container, int slot) const {
    switch (container) {
        case ContainerSlotType::Hotbar:
        case ContainerSlotType::Inventory:
        case ContainerSlotType::HotbarAndInventory:
            if (slot < 0 || slot >= CONTAINER_SIZE) {
                return nullptr;
            }
            return &mItems[(size_t) slot];

        case ContainerSlotType::Armor:
            if (slot < 0 || slot >= ARMOR_SIZE) {
                return nullptr;
            }
            return &mArmor[(size_t) slot];

        case ContainerSlotType::Offhand:
            if (slot != 0 && slot != OFFHAND_NETWORK_SLOT) {
                return nullptr;
            }
            return &mOffhand;

        case ContainerSlotType::Cursor:
            if (slot != 0) {
                return nullptr;
            }
            return &mCursor;

        case ContainerSlotType::EnchantingInput:
            return &mEnchantingInput;

        case ContainerSlotType::EnchantingMaterial:
            return &mEnchantingMaterial;

        case ContainerSlotType::AnvilInput:
            return &mAnvil[(size_t) ANVIL_INPUT];

        case ContainerSlotType::AnvilMaterial:
            return &mAnvil[(size_t) ANVIL_MATERIAL];

        case ContainerSlotType::CartographyInput:
            return &mCartography[(size_t) CARTOGRAPHY_INPUT];

        case ContainerSlotType::CartographyAdditional:
            return &mCartography[(size_t) CARTOGRAPHY_ADDITIONAL];

        // one slot each, shared by the old and the new trade screen: the slot number does not matter
        case ContainerSlotType::TradeIngredient1:
        case ContainerSlotType::Trade2Ingredient1:
            return &mTrade[0];

        case ContainerSlotType::TradeIngredient2:
        case ContainerSlotType::Trade2Ingredient2:
            return &mTrade[1];

        case ContainerSlotType::GrindstoneInput:
        case ContainerSlotType::GrindstoneAdditional: {
            const int index = grindstoneIndex(container);
            return slot == GRINDSTONE_FIRST_SLOT + index ? &mGrindstone[(size_t) index] : nullptr;
        }

        case ContainerSlotType::SmithingTableInput:
        case ContainerSlotType::SmithingTableMaterial:
        case ContainerSlotType::SmithingTableTemplate: {
            const int index = smithingIndex(container);
            return slot == SMITHING_FIRST_SLOT + index ? &mSmithing[(size_t) index] : nullptr;
        }

        case ContainerSlotType::StonecutterInput:
            if (slot != STONECUTTER_INPUT_NETWORK_SLOT) {
                return nullptr;
            }
            return &mStonecutterInput;

        case ContainerSlotType::BeaconPayment:
            if (slot != BEACON_PAYMENT_NETWORK_SLOT) {
                return nullptr;
            }
            return &mBeaconPayment;

        case ContainerSlotType::CraftingInput:
            if (slot >= CRAFTING_NETWORK_SLOT_FIRST && slot < CRAFTING_NETWORK_SLOT_FIRST + CRAFTING_SIZE) {
                return &mCrafting[(size_t) (slot - CRAFTING_NETWORK_SLOT_FIRST)];
            }
            if (slot >= CRAFTING_TABLE_NETWORK_SLOT_FIRST
                && slot < CRAFTING_TABLE_NETWORK_SLOT_FIRST + CRAFTING_TABLE_SIZE) {
                return &mCraftingTable[(size_t) (slot - CRAFTING_TABLE_NETWORK_SLOT_FIRST)];
            }
            return nullptr;

        case ContainerSlotType::FurnaceIngredient:
        case ContainerSlotType::BlastFurnaceIngredient:
        case ContainerSlotType::SmokerIngredient:
            if (slot != 0) {
                return nullptr;
            }
            return &mFurnace[0];

        case ContainerSlotType::FurnaceFuel:
            if (slot != 1) {
                return nullptr;
            }
            return &mFurnace[1];

        case ContainerSlotType::FurnaceResult:
            if (slot != 2) {
                return nullptr;
            }
            return &mFurnace[2];

        default:
            return nullptr;
    }
}

int PlayerInventory::getFirstEmptySlot() const {
    for (int slot = 0; slot < CONTAINER_SIZE; slot++) {
        if (mItems[(size_t) slot].isAir()) {
            return slot;
        }
    }
    return -1;
}

bool PlayerInventory::canStack(const ItemStack &left, const ItemStack &right) {
    if (left.isAir() || right.isAir()) {
        return false;
    }

    if (left.mDefinition->getIdentifier() != right.mDefinition->getIdentifier()) {
        return false;
    }

    if (left.mDamage != right.mDamage) {
        return false;
    }

    return left.mTag == right.mTag;
}

int PlayerInventory::getMaxStackSize(const ItemStack &item) {
    if (item.isAir())
        return DEFAULT_MAX_STACK_SIZE;

    const std::string &identifier = item.mDefinition->getIdentifier();
    const MaxStackSizeItemComponent *component =
            ItemDataTable::getComponents(identifier).get<MaxStackSizeItemComponent>();
    if (component != nullptr && component->getValue() > 0)
        return component->getValue();

    const ItemData *data = ItemDataTable::find(identifier);
    if (data == nullptr) {
        const char *blockIdentifier = item.mBlockDefinition != nullptr ? item.mBlockDefinition->getIdentifier().c_str()
                                                                       : identifier.c_str();

        const BlockData *blockData = BlockDataTable::find(blockIdentifier);
        if (blockData != nullptr && blockData->mMaxStackSize > 0)
            return blockData->mMaxStackSize;

        const int32_t customMax = CustomContentRegistry::getInstance().getItemMaxStackSize(identifier);
        return customMax > 0 ? customMax : DEFAULT_MAX_STACK_SIZE;
    }

    return data->mMaxStackSize > 0 ? data->mMaxStackSize : DEFAULT_MAX_STACK_SIZE;
}

bool PlayerInventory::addItem(const ItemStack &item) {
    std::vector<int> touchedSlots;
    return addItem(item, touchedSlots);
}

bool PlayerInventory::addItem(const ItemStack &item, std::vector<int> &outTouchedSlots) {
    return addItemPartial(item, outTouchedSlots) == 0;
}

int PlayerInventory::addItemPartial(const ItemStack &item, std::vector<int> &outTouchedSlots) {
    if (item.isAir() || item.mCount <= 0) {
        return 0;
    }

    int remaining = item.mCount;
    const int maxStackSize = getMaxStackSize(item);

    for (int slot = 0; slot < CONTAINER_SIZE && remaining > 0; slot++) {
        ItemStack &existing = mItems[(size_t) slot];
        if (!canStack(existing, item)) {
            continue;
        }

        const int free = maxStackSize - existing.mCount;
        if (free <= 0) {
            continue;
        }

        const int moved = free < remaining ? free : remaining;
        existing.mCount += moved;
        remaining -= moved;
        assignNetId(existing);
        outTouchedSlots.push_back(slot);
    }

    while (remaining > 0) {
        const int slot = getFirstEmptySlot();
        if (slot < 0) {
            break;
        }

        ItemStack copy = item;
        copy.mCount = maxStackSize < remaining ? maxStackSize : remaining;
        remaining -= copy.mCount;
        setItem(slot, std::move(copy));
        outTouchedSlots.push_back(slot);
    }

    return remaining;
}

void PlayerInventory::clear() {
    for (ItemStack &item: mItems) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mArmor) {
        item = ItemStack::air();
    }

    mOffhand = ItemStack::air();
    mBodyArmor = ItemStack::air();
    mCursor = ItemStack::air();

    for (ItemStack &item: mCrafting) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mCraftingTable) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mFurnace) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mAnvil) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mCartography) {
        item = ItemStack::air();
    }

    for (ItemStack &item: mTrade) {
        item = ItemStack::air();
    }
}

void PlayerInventory::assignNetId(ItemStack &item) {
    if (item.isAir() || item.mCount <= 0) {
        item = ItemStack::air();
        return;
    }

    item.mUsingNetId = true;
    item.mNetId = mNextNetId++;

    if (mNextNetId <= 0) {
        mNextNetId = 1;
    }
}
