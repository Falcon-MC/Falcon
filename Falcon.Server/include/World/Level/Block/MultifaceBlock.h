#pragma once

#include "World/Level/Block/ReplaceableBlock.h"

#include <string>

class MultifaceBlock final : public ReplaceableBlock {
public:
    explicit MultifaceBlock(const Block &block) : ReplaceableBlock(block) {}

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};
