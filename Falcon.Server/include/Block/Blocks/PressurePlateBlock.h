#pragma once

#include "Block/Block.h"

#include <string>

class PressurePlateBlock final : public Block {
public:
    explicit PressurePlateBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool isSignalSource() const override;

    int getSignalForEntityCount(int count) const;
};
