#pragma once

#include "Block/Block.h"

#include <string>

class RedstoneBlock final : public Block {
public:
    explicit RedstoneBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;
};
