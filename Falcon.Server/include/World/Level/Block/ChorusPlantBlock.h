#pragma once

#include "World/Level/Block/Block.h"

#include <functional>

class ChorusPlantBlock final : public Block {
public:
    explicit ChorusPlantBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool hasDynamicShape() const override {
        return true;
    }

    BlockState updateShape(Level &level, const Vector3i &position, const BlockState &state) const override;

    static BlockState connectedState(const BlockState &state,
                                     const std::function<const BlockState *(int)> &neighbour);
};
