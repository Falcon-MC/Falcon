#include "World/Actor/Mob/Component/MobShareables.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Item/Item.h"
#include "World/Level/Storage/Loot/LegacyItemMapper.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <algorithm>
#include <string>

namespace {
    const char *const SHAREABLES_COMPONENT = "minecraft:shareables";
    const char *const EQUIP_ITEM_COMPONENT = "minecraft:equip_item";
    const char *const EQUIP_BEHAVIOR = "minecraft:behavior.equip_item";
    const int32_t UNLISTED_PRIORITY = 1000;

    bool flagOr(const json::Value &entry, const char *key, bool fallback) {
        const json::Value *value = entry.get(key);
        return value == nullptr ? fallback : value->boolean(fallback);
    }

    bool flagIn(const json::Value &entry, const char *key) {
        const json::Value *value = entry.get(key);
        return value != nullptr && value->boolean(false);
    }

    int32_t integerIn(const json::Value &entry, const char *key, int32_t fallback) {
        const json::Value *value = entry.get(key);
        return value == nullptr ? fallback : value->integer(fallback);
    }

    // an entry names an item or an item tag
    bool matchesName(const json::Value *name, const ItemStack &item) {
        if (name == nullptr || !name->isString() || item.isAir())
            return false;

        const std::string identifier(item.mDefinition->getIdentifier());
        if (LegacyItemMapper::getInstance().resolveWithData(name->mString) == identifier)
            return true;

        const std::vector<std::string> &tags = CraftingRecipeTable::getItemTags(identifier);
        return std::find(tags.begin(), tags.end(), name->mString) != tags.end();
    }

    const json::Value *entryItemName(const json::Value &entry) {
        return entry.isString() ? &entry : entry.get("item");
    }
}

const json::Value *MobShareables::findEntry(const MobActor &mob, const ItemStack &item) {
    const json::Value *shareables = mob.getComponent(SHAREABLES_COMPONENT);
    const json::Value *items = shareables == nullptr ? nullptr : shareables->get("items");
    if (items == nullptr || !items->isArray() || item.isAir())
        return nullptr;

    for (const std::unique_ptr<json::Value> &entry: items->mArray) {
        if (matchesName(entryItemName(*entry), item))
            return entry.get();
    }
    return nullptr;
}

bool MobShareables::isAdmired(const MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    return entry != nullptr && flagIn(*entry, "admire");
}

bool MobShareables::isBarterCurrency(const MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    return entry != nullptr && flagIn(*entry, "barter");
}

bool MobShareables::wants(ServerNetworkHandler &owner, const MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    if (entry == nullptr || _isExcluded(mob, item))
        return false;

    const int32_t limit = integerIn(*entry, "pickup_limit", -1);
    if (limit >= 0 && _heldCount(mob, item) >= limit)
        return false;

    const int32_t maxAmount = integerIn(*entry, "max_amount", -1);
    if (maxAmount >= 0 && _heldCount(mob, item) >= maxAmount)
        return false;

    if (flagIn(*entry, "admire"))
        return mob.getAdmiration().canAdmire(owner, mob);

    if (flagIn(*entry, "consume_item"))
        return true;

    if (_equipmentSlotFor(mob, item) >= 0 && _isBetterEquipment(mob, item))
        return true;

    return flagIn(*entry, "stored_in_inventory") && mob.getInventoryCapacity() > 0;
}

bool MobShareables::take(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    if (entry == nullptr)
        return false;

    if (flagIn(*entry, "admire")) {
        if (!mob.getAdmiration().canAdmire(owner, mob))
            return false;

        mob.getAdmiration().start(owner, mob, item, flagIn(*entry, "barter"));
        return true;
    }

    if (flagIn(*entry, "consume_item"))
        return true;

    if (_equipmentSlotFor(mob, item) >= 0 && _isBetterEquipment(mob, item)) {
        if (mob.getComponent(EQUIP_BEHAVIOR) != nullptr
            && mob.getEquipment().addInventoryItem(item, mob.getInventoryCapacity()))
            return true;

        _equip(owner, mob, item);
        return true;
    }

    return flagIn(*entry, "stored_in_inventory")
           && mob.getEquipment().addInventoryItem(item, mob.getInventoryCapacity());
}

int MobShareables::findEquippableInventorySlot(const MobActor &mob) {
    const MobEquipment &equipment = mob.getEquipment();
    for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
        const ItemStack &item = equipment.getInventoryItem(slot);
        if (!item.isAir() && findEntry(mob, item) != nullptr && !_isExcluded(mob, item)
            && _isBetterEquipment(mob, item))
            return slot;
    }
    return -1;
}

void MobShareables::equipFromInventory(ServerNetworkHandler &owner, MobActor &mob, int inventorySlot) {
    MobEquipment &equipment = mob.getEquipment();
    ItemStack item = equipment.getInventoryItem(inventorySlot);
    if (item.isAir())
        return;

    ItemStack remaining = item;
    remaining.mCount -= 1;
    equipment.setInventoryItem(inventorySlot, remaining.mCount > 0 ? remaining : ItemStack::air());
    _equip(owner, mob, item);
}

bool MobShareables::store(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    if (entry != nullptr && _equipmentSlotFor(mob, item) >= 0 && _isBetterEquipment(mob, item)
        && !_isExcluded(mob, item)) {
        _equip(owner, mob, item);
        return true;
    }

    if (entry != nullptr && flagIn(*entry, "stored_in_inventory")
        && mob.getEquipment().addInventoryItem(item, mob.getInventoryCapacity()))
        return true;

    Level &level = owner.getLevelFor(mob);
    owner.dropItem(level, mob.getPosition(), item, ItemActorHandler::randomDropMotion(),
                   ItemActorHandler::DROP_PICKUP_DELAY);
    return false;
}

int32_t MobShareables::_inventoryCount(const MobActor &mob, const ItemStack &item) {
    const MobEquipment &equipment = mob.getEquipment();
    int32_t count = 0;
    for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
        const ItemStack &held = equipment.getInventoryItem(slot);
        if (!held.isAir() && held.mDefinition == item.mDefinition)
            count += held.mCount;
    }
    return count;
}

int32_t MobShareables::surplusOf(const MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    if (entry == nullptr || flagIn(*entry, "pickup_only"))
        return 0;

    const int32_t kept = integerIn(*entry, "want_amount", 0) + std::max(0, integerIn(*entry, "surplus_amount", 0));
    const int32_t held = _inventoryCount(mob, item);
    if (held <= kept)
        return 0;
    return std::min(held - kept, (int32_t) ((float) kept * (1.0f / 3.0f)));
}

int32_t MobShareables::wantedCountOf(const MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    const int32_t wanted = entry == nullptr ? 0 : integerIn(*entry, "want_amount", 0);
    if (wanted <= 0)
        return 0;
    return std::max(0, wanted - _inventoryCount(mob, item));
}

ItemStack MobShareables::sharedStack(ServerNetworkHandler &owner, const MobActor &mob, const ItemStack &item,
                                     int32_t count, int32_t &consumed) {
    consumed = 0;
    const json::Value *entry = findEntry(mob, item);
    const json::Value *craftInto = entry == nullptr ? nullptr : entry->get("craft_into");
    if (craftInto == nullptr || !craftInto->isString()) {
        ItemStack shared = item;
        shared.mCount = std::min(count, PlayerInventory::getMaxStackSize(item));
        consumed = shared.mCount;
        return shared;
    }

    const std::string input(item.mDefinition->getIdentifier());
    const std::string output = LegacyItemMapper::getInstance().resolveWithData(craftInto->mString);
    const CraftingRecipeData *recipes = CraftingRecipeTable::getRecipes();
    const CraftingIngredientData *ingredients = CraftingRecipeTable::getIngredients();
    const CraftingOutputData *outputs = CraftingRecipeTable::getOutputs();
    for (size_t index = 0; index < CraftingRecipeTable::getRecipeCount(); ++index) {
        const CraftingRecipeData &recipe = recipes[index];
        if (recipe.mOutputCount == 0 || output != outputs[recipe.mOutputOffset].mItemId)
            continue;

        int32_t needed = 0;
        for (uint32_t ingredient = 0; ingredient < recipe.mIngredientCount; ++ingredient) {
            const CraftingIngredientData &data = ingredients[recipe.mIngredientOffset + ingredient];
            if (data.mItemId != nullptr && input == data.mItemId)
                needed += std::max(1, data.mCount);
        }
        if (needed <= 0)
            continue;
        if (count < needed)
            return ItemStack::air();
        consumed = count / needed * needed;
        return owner.createItemStack(output, count / needed);
    }
    return ItemStack::air();
}

int MobShareables::_equipmentSlotFor(const MobActor &mob, const ItemStack &item) {
    if (item.isAir())
        return -1;

    // a mob that cannot wear armor carries whatever it shares in its hand: the fox and its mouth
    const json::Value *equipItem = mob.getComponent(EQUIP_ITEM_COMPONENT);
    if (equipItem != nullptr && !flagOr(*equipItem, "can_wear_armor", true))
        return MobEquipment::MAINHAND;

    const Item *type = VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
    if (type == nullptr)
        return -1;

    switch (type->getArmorSlot()) {
        case ArmorSlot::Head:
            return MobEquipment::HEAD;
        case ArmorSlot::Chest:
            return MobEquipment::CHEST;
        case ArmorSlot::Legs:
            return MobEquipment::LEGS;
        case ArmorSlot::Feet:
            return MobEquipment::FEET;
        default:
            break;
    }

    return type->getMaxStackSize() == 1 && !type->isBlock() ? MobEquipment::MAINHAND : -1;
}

int32_t MobShareables::_priorityOf(const MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    return entry == nullptr ? UNLISTED_PRIORITY : integerIn(*entry, "priority", UNLISTED_PRIORITY);
}

bool MobShareables::_isExcluded(const MobActor &mob, const ItemStack &item) {
    const json::Value *equipItem = mob.getComponent(EQUIP_ITEM_COMPONENT);
    const json::Value *excluded = equipItem == nullptr ? nullptr : equipItem->get("excluded_items");
    if (excluded == nullptr || !excluded->isArray())
        return false;

    for (const std::unique_ptr<json::Value> &entry: excluded->mArray) {
        if (matchesName(entryItemName(*entry), item))
            return true;
    }
    return false;
}

int32_t MobShareables::_heldCount(const MobActor &mob, const ItemStack &item) {
    const MobEquipment &equipment = mob.getEquipment();
    int32_t count = 0;
    for (int slot = 0; slot < MobEquipment::SLOT_COUNT; ++slot) {
        const ItemStack &held = equipment.getSlot(slot);
        if (!held.isAir() && held.mDefinition == item.mDefinition)
            count += held.mCount;
    }
    for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
        const ItemStack &held = equipment.getInventoryItem(slot);
        if (!held.isAir() && held.mDefinition == item.mDefinition)
            count += held.mCount;
    }
    return count;
}

bool MobShareables::_isBetterEquipment(const MobActor &mob, const ItemStack &item) {
    const int slot = _equipmentSlotFor(mob, item);
    if (slot < 0)
        return false;

    const ItemStack &current = mob.getEquipment().getSlot(slot);
    return current.isAir() || _priorityOf(mob, item) < _priorityOf(mob, current);
}

void MobShareables::_equip(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item) {
    const int slot = _equipmentSlotFor(mob, item);
    MobEquipment &equipment = mob.getEquipment();
    ItemStack previous = equipment.getSlot(slot);

    ItemStack equipped = item;
    equipped.mCount = 1;
    equipment.setSlot(slot, std::move(equipped));
    equipment.broadcast(owner, mob);

    if (!previous.isAir())
        owner.dropItem(owner.getLevelFor(mob), mob.getPosition(), previous, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
}
