#pragma once

#include "Block/Block.h"

#include <string>

class TurtleEggBlock : public Block {
public:
    explicit TurtleEggBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static bool isHatchingTime(Level &level);

    static void hatch(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state);
};
