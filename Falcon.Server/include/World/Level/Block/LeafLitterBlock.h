#pragma once

#include "World/Level/Block/ReplaceableBlock.h"

#include <string>

class LeafLitterBlock final : public ReplaceableBlock {
public:
    explicit LeafLitterBlock(const Block &block) : ReplaceableBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};
