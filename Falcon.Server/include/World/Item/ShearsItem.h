#pragma once

#include "World/Item/Item.h"

class ShearsItem final : public Item {
public:
    explicit ShearsItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool getDestroySpeed(const std::string &blockIdentifier, float &speed) const override;

    bool canDestroySpecial(const std::string &blockIdentifier) const override;

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;

    bool wearsOnAttack() const override {
        return false;
    }
};
