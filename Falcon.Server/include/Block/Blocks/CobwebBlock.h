#pragma once

#include "Block/Block.h"

class CobwebBlock : public Block {
public:
    explicit CobwebBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;
};
