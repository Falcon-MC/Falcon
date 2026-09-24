#pragma once

#include "Block/Block.h"
#include "Block/Blocks/WallAttachedBlock.h"

#include <string>

class LadderBlock final : public WallAttachedBlock {
public:
    explicit LadderBlock(const Block &block) : WallAttachedBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};
