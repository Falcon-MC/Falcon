#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class Level;

// a brewing stand at the end of its countdown, before any potion changes; cancelling stops the brew
struct BrewBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    const ItemStack &mIngredient;

    BrewBeforeEvent(Level &level, const Vector3i &position, const ItemStack &ingredient)
            : mLevel(level), mBlockPosition(position), mIngredient(ingredient) {}
};

// a brewing stand about to take one fuel item; starts cancelled when the fuel slot holds no blaze powder
struct BrewingFuelUseBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    ItemStack &mFuel;
    // brews the item gives
    int32_t mFuelTime;

    BrewingFuelUseBeforeEvent(Level &level, const Vector3i &position, ItemStack &fuel, int32_t fuelTime)
            : mLevel(level), mBlockPosition(position), mFuel(fuel), mFuelTime(fuelTime) {}
};

// one potion slot about to take its brewed result
struct BrewItemBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3i mBlockPosition;
    int32_t mSlot;
    const ItemStack &mInput;
    ItemStack &mResult;

    BrewItemBeforeEvent(Level &level, const Vector3i &position, int32_t slot, const ItemStack &input, ItemStack &result)
            : mLevel(level), mBlockPosition(position), mSlot(slot), mInput(input), mResult(result) {}
};

struct BrewingBeforeSignals {
    EventSignal<BrewBeforeEvent> mBrew;
    EventSignal<BrewingFuelUseBeforeEvent> mBrewingFuelUse;
    EventSignal<BrewItemBeforeEvent> mBrewItem;

    void clearBrewingBeforeSignals() {
        mBrew.clear();
        mBrewingFuelUse.clear();
        mBrewItem.clear();
    }
};

struct BrewingAfterSignals {

    void clearBrewingAfterSignals() {}
};
