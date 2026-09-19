#pragma once

#include "Actor/Actor.h"
#include "Protocol/Types/ItemStack.h"

class ItemActor : public Actor {
public:
    static const int DEFAULT_PICKUP_DELAY = 10;
    static const int LIFETIME_TICKS = 6000;
    static const int MAX_HEALTH = 5;

    ItemActor(uint64_t runtimeId, const ItemStack &item);

    const char *getIdentifier() const override { return "minecraft:item"; }

    const ItemStack &getItem() const { return mItem; }

    ItemStack &getItem() { return mItem; }

    void setItem(const ItemStack &item) { mItem = item; }

    int getPickupDelay() const { return mPickupDelay; }

    void setPickupDelay(int pickupDelay) { mPickupDelay = pickupDelay; }

    bool canPickup() const { return mPickupDelay <= 0; }

    int getAge() const { return mAge; }

    bool isExpired() const { return mAge >= LIFETIME_TICKS; }

    bool isRemoved() const { return mRemoved; }

    void setRemoved(bool removed) { mRemoved = removed; }

    void tick();

private:
    ItemStack mItem;
    int mPickupDelay = DEFAULT_PICKUP_DELAY;
    int mAge = 0;
    bool mRemoved = false;
};
