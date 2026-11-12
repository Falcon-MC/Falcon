#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class ComposterBlock final : public Block {
public:
    explicit ComposterBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;
};
