#pragma once

#include "World/Item/Item.h"

class MobActor;

class CrossbowItem : public Item {
public:
    explicit CrossbowItem(const Item &base);

    static int32_t getChargeTicks(const ItemStack &item);

    // a mob's crossbow holds its arrow in the item tag, as a player's does, so viewers see it loaded
    static void loadHeld(ServerNetworkHandler &owner, MobActor &mob);

    static void unloadHeld(ServerNetworkHandler &owner, MobActor &mob);

    int32_t getUseDuration() const override;

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    void onUsingTick(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;
};
