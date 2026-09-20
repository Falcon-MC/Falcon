#pragma once

#include "Item/Item.h"

class BoneMealItem : public Item {
public:
    explicit BoneMealItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;

private:
    static bool applyToCrop(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state);

    static bool applyToSapling(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state);

    static bool applyToGrass(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static bool applyToNylium(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state);
};
