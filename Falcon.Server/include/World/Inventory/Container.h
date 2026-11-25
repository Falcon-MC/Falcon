#pragma once

#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/ContainerSlotType.h"

class Container {
public:
    virtual ~Container() = default;

    virtual int getContainerSize() const = 0;

    // equipment precedes cargo in an entity window, but each uses its own network slot range
    virtual int getEquipmentSlotCount() const {
        return 0;
    }

    int getNetworkSlotIndex(ContainerSlotType type, int slot) const {
        if (slot < 0 || slot >= getContainerSize())
            return -1;
        const int equipment = getEquipmentSlotCount();
        if (type == ContainerSlotType::HorseEquip)
            return slot < equipment ? slot : -1;
        if (type == ContainerSlotType::LevelEntity)
            slot += equipment;
        else if (equipment != 0)
            return -1;
        return slot < getContainerSize() ? slot : -1;
    }

    virtual const ItemStack &getContainerItem(int slot) const = 0;

    virtual void setContainerItem(int slot, ItemStack item) = 0;

    virtual bool canHold(const ItemStack &item) const {
        (void) item;
        return true;
    }

    // a slot that takes only some items, as the saddle and armor slots of a mount
    virtual bool canHoldInSlot(int slot, const ItemStack &item) const {
        (void) slot;
        return canHold(item);
    }

    // whether a hopper may take the item out of the slot
    virtual bool canTakeFromSlot(int slot) const {
        (void) slot;
        return true;
    }

    // a disabled slot takes nothing, from players or hoppers alike
    virtual bool isSlotDisabled(int slot) const {
        (void) slot;
        return false;
    }
};
