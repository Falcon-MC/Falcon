#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class CopperBulbBlock final : public Block {
public:
    explicit CopperBulbBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;
};
