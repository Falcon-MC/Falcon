#pragma once

#include "Block/Block.h"

class SoulSandBlock : public Block {
public:
    explicit SoulSandBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    float getSpeedFactor() const override {
        return 0.4f;
    }
};
