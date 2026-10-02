#pragma once

#include "Block/Blocks/ChestBlock.h"

#include <string>

class TrappedChestBlock final : public ChestBlock {
public:
    explicit TrappedChestBlock(const Block &block) : ChestBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;
};
