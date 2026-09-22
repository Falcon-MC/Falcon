#pragma once

#include "Block/Blocks/LiquidBlock.h"

#include <string>

class WaterBlock : public LiquidBlock {
public:
    explicit WaterBlock(const BlockState &state) : LiquidBlock(state) {}
    explicit WaterBlock(const LiquidBlock &block) : LiquidBlock(block) {}
    explicit WaterBlock(const Block &block) : LiquidBlock(block) {}

    static bool matches(const std::string &identifier);

    int getTickRate() const override { return 5; }
    int getFlowDecayPerBlock() const override { return 1; }
    int getMinAdjacentSourcesToFormSource() const override { return 2; }
    const char *getBucketFillSound() const override { return "bucket_fill_water"; }
    const char *getBucketEmptySound() const override { return "bucket_empty_water"; }
};
