#pragma once

#include "World/Level/Block/Block.h"

class PowderSnowBlock : public Block {
public:
    explicit PowderSnowBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool getStuckMultiplier(const Actor &actor, Vector3f &multiplier) const override;

    bool freezesActorsInside() const override {
        return true;
    }

    BlockTraversal getTraversal() const override {
        return BlockTraversal::PowderSnow;
    }

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;
};
