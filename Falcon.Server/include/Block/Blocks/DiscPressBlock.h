#pragma once

#include "Block/Block.h"

class DiscPressBlock final : public Block {
public:
    explicit DiscPressBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);
};
