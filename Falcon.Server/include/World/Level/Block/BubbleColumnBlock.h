#pragma once

#include "World/Level/Block/LiquidBlock.h"

class BubbleColumnBlock : public LiquidBlock {
public:
    explicit BubbleColumnBlock(const BlockState &state) : LiquidBlock(state) {}

    int getTickRate() const override { return 5; }
};
