#pragma once

#include "World/Item/Item.h"

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

    bool getLungeImpulse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                         Vector3f &impulse) const;

    void applyLunge(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const;

    Actor *findTarget(ServerNetworkHandler &owner, ServerPlayer &player, float maxDistance) const;

    Actor *findSweepTarget(ServerNetworkHandler &owner, ServerPlayer &player) const;

    // how fast the holder closes in on the target along its look, in blocks a second
    float closingSpeedOf(ServerNetworkHandler &owner, const ServerPlayer &player, const Actor &target) const;

    void applySpearDamage(ServerNetworkHandler &owner, ServerPlayer &attacker, Actor &target, float damage) const;

    std::string mTierName;
};
