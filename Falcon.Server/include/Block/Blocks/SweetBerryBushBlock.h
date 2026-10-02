#pragma once

#include "Block/Block.h"

class SweetBerryBushBlock : public Block {
public:
    explicit SweetBerryBushBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool getStuckMultiplier(const Actor &actor, Vector3f &multiplier) const override;

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;
};
