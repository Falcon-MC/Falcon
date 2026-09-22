#pragma once

#include "Item/Item.h"

#include <string>

class ServerActor;

class BowItem : public Item {
public:
    explicit BowItem(const Item &base);

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;
};

class CrossbowItem : public Item {
public:
    explicit CrossbowItem(const Item &base);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    void onUsingTick(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;
};

class TridentItem : public Item {
public:
    explicit TridentItem(const Item &base);

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;

private:
    bool applyRiptide(ServerNetworkHandler &owner, ServerPlayer &player, int32_t level) const;
};
