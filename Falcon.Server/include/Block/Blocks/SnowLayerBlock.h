#pragma once

#include "Block/Block.h"

#include <string>

class SnowLayerBlock final : public Block {
public:
    explicit SnowLayerBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canBeReplaced(const BlockState &state) const override;

    PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                        const Vector3f &clickPosition, Vector3i &position,
                                        BlockState &state) const override;

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
