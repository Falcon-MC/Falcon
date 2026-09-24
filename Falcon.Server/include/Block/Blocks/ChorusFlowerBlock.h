#pragma once

#include "Block/Block.h"

class ChorusFlowerBlock : public Block {
public:
    explicit ChorusFlowerBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static bool allNeighboursEmpty(Level &level, const Vector3i &position, int32_t exceptFace);

    static void placeFlower(Level &level, const Vector3i &position, int32_t age);
};
