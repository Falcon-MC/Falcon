#pragma once

#include "Block/Block.h"

#include <string>

class RedstoneLampBlock final : public Block {
public:
    explicit RedstoneLampBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool isLit() const;

    static BlockState getLitState(const BlockState &state);

    static BlockState getUnlitState(const BlockState &state);
};
