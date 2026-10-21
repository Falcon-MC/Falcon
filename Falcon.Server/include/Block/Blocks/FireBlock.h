#pragma once

#include "Block/Block.h"

#include <string>

class FireBlock final : public Block {
public:
    explicit FireBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool canBeReplaced(const BlockState &state) const override;

    bool hasDynamicShape() const override {
        return true;
    }

    BlockState updateShape(Level &level, const Vector3i &position, const BlockState &state) const override;

    PathHazard getPathHazard() const override {
        return PathHazard::Damaging;
    }
};
