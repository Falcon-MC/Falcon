#pragma once

#include "Item/Item.h"

#include <string>

class Actor;
class ServerActor;

class SpearItem : public Item {
public:
    explicit SpearItem(const Item &base);

    bool onStartUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

    void onUsingTick(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;

    bool onStopUsing(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                     int32_t elapsedTicks) const override;

    static bool matches(const std::string &identifier);

    float getKineticDamage(float speed) const;

    const std::string &getTierName() const {
        return mTierName;
    }

private:
    void stab(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const;

    float getJabDamage(const ItemStack &item) const;

    void applyLunge(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const;

    Actor *findTarget(ServerNetworkHandler &owner, ServerPlayer &player, float maxDistance) const;

    Actor *findSweepTarget(ServerNetworkHandler &owner, ServerPlayer &player) const;

    void applySpearDamage(ServerNetworkHandler &owner, ServerPlayer &attacker, Actor &target, float damage) const;

    std::string mTierName;
};
