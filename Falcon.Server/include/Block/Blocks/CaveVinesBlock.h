#pragma once

#include "Block/Block.h"

class CaveVinesBlock : public Block {
public:
    explicit CaveVinesBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
