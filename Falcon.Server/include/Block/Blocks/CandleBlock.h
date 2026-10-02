#pragma once

#include "Block/Block.h"

#include <string>

class CandleBlock final : public Block {
public:
    explicit CandleBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                        const Vector3f &clickPosition, Vector3i &position,
                                        BlockState &state) const override;
};
