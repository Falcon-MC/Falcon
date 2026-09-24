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
