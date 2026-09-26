#include "Actor/Mob/Component/MobShareables.h"

#include "Actor/Mob/MobActor.h"
#include "Item/Item.h"
#include "Item/Loot/LegacyItemMapper.h"
#include "Item/VanillaItems.h"
#include "Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <string>

namespace {
    const char *const SHAREABLES_COMPONENT = "minecraft:shareables";
    const char *const EQUIP_ITEM_COMPONENT = "minecraft:equip_item";
    const int32_t UNLISTED_PRIORITY = 1000;

    bool flagIn(const json::Value &entry, const char *key) {
        const json::Value *value = entry.get(key);
        return value != nullptr && value->boolean(false);
    }

    int32_t integerIn(const json::Value &entry, const char *key, int32_t fallback) {
        const json::Value *value = entry.get(key);
        return value == nullptr ? fallback : value->integer(fallback);
    }

    bool matchesName(const json::Value *name, const ItemStack &item) {
        return name != nullptr && name->isString() && !item.isAir()
               && LegacyItemMapper::getInstance().resolveWithData(name->mString) == item.mDefinition->getIdentifier();
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

    if (_equipmentSlotFor(item) >= 0 && _isBetterEquipment(mob, item))
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

    if (_equipmentSlotFor(item) >= 0 && _isBetterEquipment(mob, item)) {
        _equip(owner, mob, item);
        return true;
    }

    return flagIn(*entry, "stored_in_inventory") && mob.getEquipment().addInventoryItem(item,
                                                                                         mob.getInventoryCapacity());
}

bool MobShareables::store(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item) {
    const json::Value *entry = findEntry(mob, item);
    if (entry != nullptr && _equipmentSlotFor(item) >= 0 && _isBetterEquipment(mob, item) && !_isExcluded(mob, item)) {
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

int MobShareables::_equipmentSlotFor(const ItemStack &item) {
    if (item.isAir())
        return -1;

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
    const int slot = _equipmentSlotFor(item);
    if (slot < 0)
        return false;

    const ItemStack &current = mob.getEquipment().getSlot(slot);
    return current.isAir() || _priorityOf(mob, item) < _priorityOf(mob, current);
}

void MobShareables::_equip(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item) {
    const int slot = _equipmentSlotFor(item);
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
