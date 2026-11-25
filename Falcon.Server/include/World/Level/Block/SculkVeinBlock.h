#pragma once

#include "World/Level/Block/ReplaceableBlock.h"

#include <string>

class SculkVeinBlock final : public ReplaceableBlock {
public:
    explicit SculkVeinBlock(const Block &block) : ReplaceableBlock(block) {}

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
};
