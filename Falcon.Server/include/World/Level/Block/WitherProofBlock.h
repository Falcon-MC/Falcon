#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// the blocks the wither never breaks, hurt or charging; command blocks and the end portal hold through their own
// classes
class WitherProofBlock final : public Block {
public:
    explicit WitherProofBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool resistsWither(bool charging) const override {
        (void) charging;
        return true;
    }
};
