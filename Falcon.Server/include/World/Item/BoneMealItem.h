#pragma once

#include "World/Item/Item.h"

class BoneMealItem : public Item {
public:
    explicit BoneMealItem(const Item &base);

    static bool matches(const std::string &identifier);

    bool onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                      const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const override;

    bool dispense(const DispenseSource &source) const override;

private:
    static constexpr int USE_COOLDOWN_TICKS = 4;

    // false: the block takes no bone meal
    static bool fertilize(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static bool applyToCrop(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state);

    static bool applyToSapling(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state);

    static bool applyToGrass(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    static bool applyToNylium(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state);
};
