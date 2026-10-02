#pragma once

#include "Block/Block.h"

#include <string>

class CarpetBlock final : public Block {
public:
    explicit CarpetBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};
