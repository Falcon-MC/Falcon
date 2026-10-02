#pragma once

#include "Block/Block.h"

class IceCrystalBlock : public Block {
public:
    explicit IceCrystalBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};
