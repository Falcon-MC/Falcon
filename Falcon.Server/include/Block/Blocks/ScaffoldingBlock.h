#pragma once

#include "Block/Block.h"

#include <string>

class ScaffoldingBlock final : public Block {
public:
    explicit ScaffoldingBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isClimbable() const override {
        return true;
    }

    Vector3i resolvePlacementPosition(Level &level, const Vector3i &position, int blockFace) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    void onPlacing(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                   BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};
