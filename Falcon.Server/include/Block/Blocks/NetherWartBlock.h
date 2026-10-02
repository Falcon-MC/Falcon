#pragma once

#include "Block/Blocks/CropBlock.h"

class NetherWartBlock : public CropBlock {
public:
    using CropBlock::CropBlock;

    static bool matches(const std::string &identifier);

protected:
    const char *getGrowthState() const override { return "age"; }

    int getMaxGrowth() const override { return 3; }

    int getGrowthChance() const override { return 10; }

    bool needsLight() const override { return false; }
};
