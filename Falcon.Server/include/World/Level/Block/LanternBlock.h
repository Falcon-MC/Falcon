#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class LanternBlock final : public Block {
public:
    explicit LanternBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);
};
