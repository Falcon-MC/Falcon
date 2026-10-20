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

private:
    static int _equipmentSlotFor(const ItemStack &item);

    static int32_t _priorityOf(const MobActor &mob, const ItemStack &item);

    static bool _isExcluded(const MobActor &mob, const ItemStack &item);

    static int32_t _heldCount(const MobActor &mob, const ItemStack &item);

    static bool _isBetterEquipment(const MobActor &mob, const ItemStack &item);

    static void _equip(ServerNetworkHandler &owner, MobActor &mob, const ItemStack &item);
};
