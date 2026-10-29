#pragma once

#include "World/Level/Block/ContainerBlock.h"

class DispenserBlock : public ContainerBlock {
public:
    static constexpr int64_t TRIGGER_DELAY_TICKS = 4;

    explicit DispenserBlock(const Block &base);

    static bool matches(const std::string &identifier);

    static void onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

private:
    static void _eject(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int facing,
                       const ItemStack &item);
};
