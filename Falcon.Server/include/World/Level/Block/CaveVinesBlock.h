#pragma once

#include "World/Level/Block/Block.h"

class CaveVinesBlock : public Block {
public:
    explicit CaveVinesBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    bool isClimbable() const override {
        return true;
    }

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }
};
