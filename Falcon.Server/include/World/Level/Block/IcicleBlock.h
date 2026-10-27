#pragma once

#include "World/Level/Block/Block.h"

class IcicleBlock final : public Block {
public:
    explicit IcicleBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;
};
