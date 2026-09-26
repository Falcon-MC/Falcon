#pragma once

#include "Block/Block.h"

class NetherVinesBlock : public Block {
public:
    explicit NetherVinesBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool isClimbable() const override {
        return true;
    }
};
