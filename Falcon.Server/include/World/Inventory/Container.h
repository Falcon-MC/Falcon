#pragma once

#include "Protocol/Types/ItemStack.h"

class Container {
public:
    virtual ~Container() = default;

    virtual int getContainerSize() const = 0;

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

    // a disabled slot takes nothing, from players or hoppers alike
    virtual bool isSlotDisabled(int slot) const {
        (void) slot;
        return false;
    }
};
