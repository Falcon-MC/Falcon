#pragma once

#include "Block/Block.h"

#include <string>

class DoubleSlabBlock final : public Block {
public:
    explicit DoubleSlabBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    std::string getSlabIdentifier() const;

    std::string getResourceItem(const BlockState &state) const override;

    int32_t getResourceCount(const BlockState &state) const override;
};
