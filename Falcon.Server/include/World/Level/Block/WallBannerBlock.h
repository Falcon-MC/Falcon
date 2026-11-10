#pragma once

#include "World/Level/Block/WallAttachedBlock.h"

#include <string>

class WallBannerBlock final : public WallAttachedBlock {
public:
    explicit WallBannerBlock(const Block &block) : WallAttachedBlock(block) {}

    static bool matches(const std::string &identifier);
};
