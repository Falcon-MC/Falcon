#pragma once

#include "World/Level/Block/Block.h"

class BuddingAmethystBlock : public Block {
public:
    explicit BuddingAmethystBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }
};
