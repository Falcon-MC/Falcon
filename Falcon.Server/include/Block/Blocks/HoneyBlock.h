#pragma once

#include "Block/Block.h"

class HoneyBlock : public Block {
public:
    explicit HoneyBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    float getSpeedFactor() const override {
        return 0.4f;
    }

    float getJumpFactor() const override {
        return 0.5f;
    }

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;
};
