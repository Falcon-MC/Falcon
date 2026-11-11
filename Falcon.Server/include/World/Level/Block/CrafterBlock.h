#pragma once

#include "World/Level/Block/ContainerBlock.h"

#include <string>

class CrafterBlock final : public ContainerBlock {
public:
    static constexpr int64_t TRIGGER_DELAY_TICKS = 4;

    explicit CrafterBlock(const Block &block) : ContainerBlock(block) {}

    static bool matches(const std::string &identifier);

    // sub_14772D900: a rising edge sets triggered_bit and asks for the craft 4 ticks later
    static void onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);

    // sub_14772C510: crafts the grid's recipe once
    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;
};
