#pragma once

#include "World/Item/Item.h"

#include <cstdint>

class MaceItem : public Item {
public:
    static const int32_t BASE_ATTACK_DAMAGE = 6;

    // upper bound on the fall height used for the smash bonus; past any legitimate world-height fall it only
    // caps a client-controlled value, it does not change damage for real falls
    static const int32_t MAX_SMASH_FALL = 1024;

    explicit MaceItem(const Item &base);

    float getAttackDamageBonus(const ItemStack &item, ServerPlayer &attacker) const override;

    float getArmorEfficiency(const ItemStack &item) const override;

    void onPostAttack(ServerNetworkHandler &owner, ServerPlayer &attacker, Actor &victim, float damage,
                      const ItemStack &item) const override;

    static int32_t computeSmashDamage(int32_t fallHeight);

private:
    static void applyWindBurst(ServerNetworkHandler &owner, ServerPlayer &attacker, int32_t level);
};
