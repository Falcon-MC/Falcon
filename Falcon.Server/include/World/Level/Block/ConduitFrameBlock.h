#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// prismarine, prismarine bricks, dark prismarine and sea lantern
class ConduitFrameBlock final : public Block {
public:
    explicit ConduitFrameBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool isConduitFrame() const override {
        return true;
    }
};
