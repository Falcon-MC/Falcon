#pragma once

#include "World/Level/Block/Block.h"

class CauldronBlock final : public Block {
public:
    enum class Liquid {
        Empty,
        Water,
        Lava
    };

    explicit CauldronBlock(const BlockState &state) : Block(state) {}

    int getFillLevel() const;

    Liquid getLiquid() const;

    bool isFull() const;

    bool canFill(Liquid liquid) const;

    BlockState withFillLevel(Liquid liquid, int fillLevel) const;

    static BlockState makeState(Liquid liquid, int fillLevel);
};
