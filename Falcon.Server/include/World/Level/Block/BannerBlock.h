#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class BannerBlock final : public Block {
public:
    explicit BannerBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);
};
