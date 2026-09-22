#pragma once

#include "Block/Block.h"

#include <string>

class FireBlock final : public Block {
public:
    explicit FireBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool canBeReplaced(const BlockState &state) const override;
};
