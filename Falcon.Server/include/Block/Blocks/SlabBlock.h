#pragma once

#include "Block/Block.h"

#include <string>

class SlabBlock final : public Block {
public:
    explicit SlabBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    static bool isTopSlab(const BlockState &state);

    std::string getDoubleSlabIdentifier() const;

    PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                        const Vector3f &clickPosition, Vector3i &position,
                                        BlockState &state) const override;

    bool getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const override;
};
