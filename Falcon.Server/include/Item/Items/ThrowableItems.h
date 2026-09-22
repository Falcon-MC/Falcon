#pragma once

#include "Item/Item.h"

#include <string>

class ThrowableItem : public Item {
public:
    ThrowableItem(const Item &base, std::string entityIdentifier, float throwForce, int32_t cooldownTicks);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;

protected:
    std::string mEntityIdentifier;
    float mThrowForce;
    int32_t mCooldownTicks;
};

class ThrownPotionItem : public ThrowableItem {
public:
    ThrownPotionItem(const Item &base, std::string entityIdentifier);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};

class SpawnEggItem : public Item {
public:
    explicit SpawnEggItem(const Item &base);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;
};
