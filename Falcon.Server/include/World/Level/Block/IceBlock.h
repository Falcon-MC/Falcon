#pragma once

#include "World/Level/Block/Block.h"

class IceBlock : public Block {
public:
    explicit IceBlock(const Block &block) : Block(block) {
    }

    static constexpr const char *IDENTIFIER = "minecraft:ice";

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }
};
