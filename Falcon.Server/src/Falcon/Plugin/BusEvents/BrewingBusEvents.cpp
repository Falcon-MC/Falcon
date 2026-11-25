#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Event/EventBus.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Types/ItemStack.h"

#include <algorithm>

void PluginBusEvents::hookBrewing(PluginManager &plugins, EventBus &bus) {
    BrewingBeforeSignals &before = bus.before();

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BREW), before.mBrew, [&plugins](BrewBeforeEvent &source) {
        ItemStack ingredient = source.mIngredient;
        PluginEvent event;
        event.mType = FALCON_EVENT_BREW;
        event.mCancellable = true;
        event.mCancelled = source.isCancelled();
        event.mLevel = &source.mLevel;
        event.mBlockPosition = source.mBlockPosition;
        event.mItem = &ingredient;
        plugins.dispatch(event);
        source.setCancelled(event.mCancelled);
    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BREWING_FUEL_USE), before.mBrewingFuelUse,
                    [&plugins](BrewingFuelUseBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BREWING_FUEL_USE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mAmount = source.mFuelTime;
                        event.mItem = &source.mFuel;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled || !(event.mAmount > 0.0));
                        // the stand keeps its fuel in a short
                        if (!source.isCancelled())
                            source.mFuelTime = (int32_t) std::min(event.mAmount, 32767.0);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BREW_ITEM), before.mBrewItem,
                    [&plugins](BrewItemBeforeEvent &source) {
                        ItemStack input = source.mInput;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BREW_ITEM;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mSourceSlot = source.mSlot;
                        event.mItem = &input;
                        event.mResult = &source.mResult;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });
}
