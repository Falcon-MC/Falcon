#pragma once

#include "Core/Json/Json.h"

#include <cstdint>

class ItemStack;
class MobActor;
class ServerNetworkHandler;

class MobShareables {
public:
    static const json::Value *findEntry(const MobActor &mob, const ItemStack &item);

    static bool wants(ServerNetworkHandler &owner, const MobActor &mob, const ItemStack &item);

    static bool take(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item);

    static bool store(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item);

    static bool isBarterCurrency(const MobActor &mob, const ItemStack &item);

    static int findEquippableInventorySlot(const MobActor &mob);

    static void equipFromInventory(ServerNetworkHandler &owner, MobActor &mob, int inventorySlot);

    // share_items: what the inventory holds past want_amount + surplus_amount, at most a third of that sum;
    // 0 for an unlisted or pickup_only item
    static int32_t surplusOf(const MobActor &mob, const ItemStack &item);

    // want_amount less what the inventory holds, never below 0
    static int32_t wantedCountOf(const MobActor &mob, const ItemStack &item);

    // the stack a surplus of count items is given as: the entry's craft_into item, one per recipe's worth of
    // the item, when it names one; consumed: how many of the item it takes
    static ItemStack sharedStack(ServerNetworkHandler &owner, const MobActor &mob, const ItemStack &item,
                                 int32_t count, int32_t &consumed);

private:
    static int _equipmentSlotFor(const MobActor &mob, const ItemStack &item);

    static int32_t _priorityOf(const MobActor &mob, const ItemStack &item);

    static bool _isExcluded(const MobActor &mob, const ItemStack &item);

    static int32_t _heldCount(const MobActor &mob, const ItemStack &item);

    static int32_t _inventoryCount(const MobActor &mob, const ItemStack &item);

    static bool _isBetterEquipment(const MobActor &mob, const ItemStack &item);

    static void _equip(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item);
};
