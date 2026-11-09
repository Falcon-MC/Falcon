#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class GrassPathBlock : public Block {
public:
    explicit GrassPathBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const override;
};
